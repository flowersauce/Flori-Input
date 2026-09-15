/**
 * @file clicker_runtime.cpp
 * @brief 实现触发器控制器的线程安全运行时适配。
 */
#include "app/clicker_runtime.h"

#include <atomic>
#include <stdexcept>
#include <utility>

#include "platform/keyboard_hook.h"

namespace flori_input
{
	struct ClickerRuntime::State
	{
		Dispatcher						   dispatcher;
		Changed							   changed;
		KeyObserved						   keyObserved;
		KeyRoute						   keyRoute;
		CoordinateSelection				   coordinateSelection;
		std::atomic<bool>				   active{true};
		std::atomic<std::uint64_t>		   keyGeneration{};
		std::unique_ptr<ClickerController> controller;
		std::unique_ptr<KeyboardHook>	   hook;

		/** @brief 向所有者线程广播控制器状态变化。 */
		void notify() const
		{
			if (active && changed)
			{
				changed(*controller);
			}
		}

		/** @brief 仅在运行时状态仍存活时投递任务。 */
		static void post(const std::weak_ptr<State> &weak, std::function<void(State &)> operation)
		{
			if (const auto strong = weak.lock(); strong && strong->active)
			{
				strong->dispatcher(
					[weak, operation = std::move(operation)]
					{
						if (const auto target = weak.lock(); target && target->active)
						{
							operation(*target);
						}
					});
			}
		}
	};

	ClickerRuntime::ClickerRuntime(Dispatcher				dispatcher,
								   Changed					changed,
								   KeyObserved				keyObserved,
								   KeyRoute					keyRoute,
								   ClickerController::Start start,
								   ClickerController::Stop	stop,
								   CoordinateMove			coordinateMove)
		: state(std::make_shared<State>())
	{
		if (!dispatcher)
		{
			throw std::invalid_argument("[[runtime.dispatcher_required]]");
		}
		state->dispatcher  = std::move(dispatcher);
		state->changed	   = std::move(changed);
		state->keyObserved = std::move(keyObserved);
		state->keyRoute	   = std::move(keyRoute);
		if (!state->keyRoute)
		{
			state->keyRoute = [](ClickerController &controller, const std::uint32_t key) { controller.handleKeyPressed(key); };
		}
		if (!start || !stop)
		{
			throw std::invalid_argument("[[runtime.trigger_required]]");
		}
		state->controller = std::make_unique<ClickerController>(std::move(start), std::move(stop));
		state->hook		  = std::make_unique<KeyboardHook>(
			  keyCallback(),
			  [weak = std::weak_ptr<State>(state)](const ScreenPoint point, const bool picked)
			  {
				  State::post(weak,
							  [point, picked](State &target)
							  {
								  const auto capture = target.controller->capture();
								  if (capture != ClickerController::Capture::Coordinate && capture != ClickerController::Capture::CoordinateSelection)
								  {
									  return;
								  }
								  if (picked)
								  {
									  if (capture == ClickerController::Capture::CoordinateSelection)
									  {
										  target.controller->cancelCoordinateCapture();
										  if (target.coordinateSelection)
										  {
											  target.coordinateSelection(point);
										  }
									  }
									  else
									  {
										  target.controller->finishCoordinateCapture(point);
									  }
								  }
								  else
								  {
									  target.controller->cancelCoordinateCapture();
								  }
								  target.notify();
							  });
			  },
			  std::move(coordinateMove));
	}

	ClickerRuntime::~ClickerRuntime()
	{
		shutdown();
	}

	const ClickerController &ClickerRuntime::controller() const
	{
		return *state->controller;
	}

	void ClickerRuntime::update(const std::function<void(ClickerController &)> &operation) const
	{
		const auto strong = state;
		if (!strong->active)
		{
			return;
		}
		operation(*strong->controller);
		strong->notify();
	}

	bool ClickerRuntime::installHooks() const
	{
		return state->active && state->hook->install();
	}
	bool ClickerRuntime::hooksInstalled() const
	{
		return state->hook && state->hook->isInstalled();
	}
	void ClickerRuntime::setCoordinateCaptureActive(const bool active) const
	{
		if (state->active && state->hook)
		{
			state->hook->setCoordinateCaptureActive(active);
		}
	}
	void ClickerRuntime::setCoordinateSelectionHandler(CoordinateSelection handler) const
	{
		state->coordinateSelection = std::move(handler);
	}

	ClickerRuntime::KeyCallback ClickerRuntime::keyCallback() const
	{
		return [weak = std::weak_ptr<State>(state)](std::uint32_t key)
		{
			if (const auto strong = weak.lock(); strong && strong->active && strong->keyObserved)
			{
				try
				{
					strong->keyObserved(key);
				}
				catch (...)
				{
				}
			}
			const auto source = weak.lock();
			if (!source)
			{
				return;
			}
			const auto epoch = source->keyGeneration.load();
			State::post(weak,
						[key, epoch](const State &target)
						{
							if (epoch != target.keyGeneration.load())
							{
								return;
							}
							target.keyRoute(*target.controller, key);
							target.notify();
						});
		};
	}
	void ClickerRuntime::invalidateKeyEvents() const
	{
		++state->keyGeneration;
	}

	void ClickerRuntime::shutdown() const
	{
		if (!state->active)
		{
			return;
		}
		state->active = false;
		state->hook.reset();
		state->controller->stop();
		state->controller->executionFinished();
	}
} // namespace flori_input
