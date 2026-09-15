/**
 * @file windows_trigger_executor.cpp
 * @brief 实现高精度、低开销的 Windows 触发器执行循环。
 */
#include "platform/windows_trigger_executor.h"
#include "platform/windows_input_marker.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>

namespace flori_input::platform
{
	namespace
	{
		class ScopedThreadPriority final
		{
		public:
			/** @brief 临时提高当前工作线程优先级。 */
			ScopedThreadPriority()
				: thread_(GetCurrentThread())
				, previous_(GetThreadPriority(thread_))
			{
				SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
			}
			/** @brief 恢复线程进入作用域前的优先级。 */
			~ScopedThreadPriority()
			{
				if (previous_ != THREAD_PRIORITY_ERROR_RETURN)
				{
					SetThreadPriority(thread_, previous_);
				}
			}

			ScopedThreadPriority(const ScopedThreadPriority &)			  = delete;
			ScopedThreadPriority &operator=(const ScopedThreadPriority &) = delete;

		private:
			HANDLE thread_;
			int	   previous_;
		};

		struct InputSequence
		{
			std::array<INPUT, 2> events{};
			std::size_t			 count{};
			bool				 canHold{};
		};

		/** @brief 将屏幕物理坐标映射到 SendInput 绝对坐标。 */
		int absoluteCoordinate(const int value, const int origin, const int size)
		{
			if (size <= 1)
			{
				return 0;
			}
			return std::clamp(MulDiv(value - origin, 65535, size - 1), 0, 65535);
		}

		/** @brief 在进入高频循环前预构造按下与释放事件。 */
		InputSequence inputSequence(const InjectorConfig &config)
		{
			InputSequence sequence;
			sequence.count	 = config.action == InputAction::Hold ? 1 : 2;
			sequence.canHold = true;
			switch (config.inputKey)
			{
				case InputKey::MouseLeft:
					sequence.events[0].type		  = INPUT_MOUSE;
					sequence.events[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
					sequence.events[1].type		  = INPUT_MOUSE;
					sequence.events[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
					break;
				case InputKey::MouseMiddle:
					sequence.events[0].type		  = INPUT_MOUSE;
					sequence.events[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
					sequence.events[1].type		  = INPUT_MOUSE;
					sequence.events[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
					break;
				case InputKey::MouseRight:
					sequence.events[0].type		  = INPUT_MOUSE;
					sequence.events[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
					sequence.events[1].type		  = INPUT_MOUSE;
					sequence.events[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
					break;
				case InputKey::Custom:
					if (isWheelKey(config.customKey))
					{
						sequence.count					= 1;
						sequence.canHold				= false;
						sequence.events[0].type			= INPUT_MOUSE;
						sequence.events[0].mi.dwFlags	= MOUSEEVENTF_WHEEL;
						sequence.events[0].mi.mouseData = config.customKey == WheelUpKey ? WHEEL_DELTA : static_cast<DWORD>(-WHEEL_DELTA);
					}
					else
					{
						sequence.events[0].type		  = INPUT_KEYBOARD;
						sequence.events[0].ki.wVk	  = static_cast<WORD>(config.customKey);
						sequence.events[1].type		  = INPUT_KEYBOARD;
						sequence.events[1].ki.wVk	  = static_cast<WORD>(config.customKey);
						sequence.events[1].ki.dwFlags = KEYEVENTF_KEYUP;
					}
					break;
			}

			if (config.cursorMode == CursorMode::Locked && config.inputKey != InputKey::Custom)
			{
				sequence.events[0].mi.dwFlags |= MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
				sequence.events[0].mi.dx =
					absoluteCoordinate(config.lockedCoordinate.x, GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN));
				sequence.events[0].mi.dy =
					absoluteCoordinate(config.lockedCoordinate.y, GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
			}
			for (auto &event: sequence.events)
			{
				if (event.type == INPUT_KEYBOARD)
				{
					event.ki.dwExtraInfo = InjectedInputMarker;
				}
				else
				{
					event.mi.dwExtraInfo = InjectedInputMarker;
				}
			}
			return sequence;
		}

		/** @brief 按截止时间等待，并在取消时提前返回。 */
		void waitUntil(const std::chrono::steady_clock::time_point deadline, const std::stop_token &stop)
		{
			using namespace std::chrono_literals;
			while (!stop.stop_requested())
			{
				const auto remaining = deadline - std::chrono::steady_clock::now();
				if (remaining <= std::chrono::steady_clock::duration::zero())
				{
					return;
				}
				if (remaining > 10ms)
				{
					std::this_thread::sleep_for(1ms);
				}
				else
				{
					YieldProcessor();
				}
			}
		}
	} // namespace

	ExecutionTask makeWindowsTriggerTask(const InjectorConfig &config, NativeInputSender sender)
	{
		if (config.pressDuration < std::chrono::nanoseconds::zero() || config.pressDuration > MaximumPressDuration || config.cycle < minimumInputCycle(config)
			|| config.cycle > MaximumInputCycle || !std::isfinite(config.timingJitterPercent) || config.timingJitterPercent < 0
			|| config.timingJitterPercent > MaximumTimingJitterPercent || static_cast<unsigned>(config.inputKey) > 3 || static_cast<unsigned>(config.action) > 1
			|| static_cast<unsigned>(config.cursorMode) > 1
			|| (config.inputKey == InputKey::Custom && !isWheelKey(config.customKey)
				&& (config.customKey == 0 || config.customKey > std::numeric_limits<WORD>::max())))
		{
			throw std::invalid_argument("[[trigger.invalid_config]]");
		}

		if (!sender)
		{
			sender = [](std::span<const INPUT> events)
			{ return SendInput(static_cast<UINT>(events.size()), const_cast<INPUT *>(events.data()), sizeof(INPUT)); };
		}

		const auto sequence		   = inputSequence(config);
		const auto requestedJitter = std::chrono::nanoseconds(
			static_cast<std::chrono::nanoseconds::rep>(static_cast<double>(config.cycle.count()) * config.timingJitterPercent / 100.0));
		const auto jitterAmplitude = std::min(requestedJitter, config.cycle - minimumInputCycle(config));

		return [config, sequence, jitterAmplitude, sender = std::move(sender)](const std::stop_token &stop)
		{
			const ScopedThreadPriority priority;
			if (config.action == InputAction::Hold && sequence.canHold)
			{
				sender(std::span(sequence.events.data(), 1));
				while (!stop.stop_requested())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				sender(std::span(sequence.events.data() + 1, 1));
				return;
			}

			std::mt19937_64						   random(static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
									  ^ static_cast<std::uint64_t>(GetCurrentThreadId()));
			std::uniform_real_distribution<double> factor(-1.0, 1.0);
			auto								   deadline = std::chrono::steady_clock::now();
			while (!stop.stop_requested())
			{
				waitUntil(deadline, stop);
				if (stop.stop_requested())
				{
					break;
				}

				if (sequence.count == 2 && usesTimedPress(config))
				{
					sender(std::span(sequence.events.data(), 1));
					waitUntil(std::chrono::steady_clock::now() + config.pressDuration, stop);
					sender(std::span(sequence.events.data() + 1, 1));
				}
				else
				{
					sender(std::span(sequence.events.data(), sequence.count));
				}
				if (stop.stop_requested())
				{
					break;
				}

				if (const auto now = std::chrono::steady_clock::now(); deadline < now - config.cycle)
				{
					deadline = now;
				}
				deadline += config.cycle;
				if (jitterAmplitude != std::chrono::nanoseconds::zero())
				{
					deadline +=
						std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(static_cast<double>(jitterAmplitude.count()) * factor(random)));
				}
			}
		};
	}
} // namespace flori_input::platform
