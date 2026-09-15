/**
 * @file program_executor.cpp
 * @brief 实现输入程序的顺序、延时和循环执行。
 */
#include "core/program_executor.h"

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>
#include <utility>

namespace flori_input
{
	ExecutionClock::TimePoint SteadyExecutionClock::now() const
	{
		return std::chrono::steady_clock::now();
	}
	bool SteadyExecutionClock::waitUntil(TimePoint deadline, std::stop_token stop)
	{
		if (deadline <= now())
		{
			return !stop.stop_requested();
		}
		std::mutex					mutex;
		std::condition_variable_any changed;
		std::unique_lock			lock(mutex);
		changed.wait_until(lock, stop, deadline, [] { return false; });
		return !stop.stop_requested();
	}

	ExecutionResult executeProgram(const Program							 &program,
								   InputBackend								 &backend,
								   ExecutionClock							 &clock,
								   const std::stop_token					 &stop,
								   const std::optional<ScriptExecutionLimits> scriptLimits)
	{
		const std::optional<ScriptExecutionLimits> effectiveLimits =
			scriptLimits ? std::optional<ScriptExecutionLimits>{normalizedScriptExecutionLimits(*scriptLimits)} : std::nullopt;
		const bool finiteScript = effectiveLimits
			&& std::ranges::none_of(program.nodes(),
									[](const ProgramNode &node)
									{
										const auto *begin = std::get_if<RepeatBegin>(&node);
										return begin && !begin->count;
									});
		std::uint64_t totalSteps{};
		std::uint64_t consecutiveNoWaitSteps{};
		const auto	  accountStep = [&](const bool waits = false)
		{
			if (!effectiveLimits)
			{
				return;
			}
			if (finiteScript && ++totalSteps > effectiveLimits->maxFiniteSteps)
			{
				throw InputError("[[execution.step_limit]]");
			}
			if (!waits && ++consecutiveNoWaitSteps > effectiveLimits->maxConsecutiveNoWaitSteps)
			{
				throw InputError("[[execution.no_wait_limit]]");
			}
		};
		ExecutionResult result;
		const auto		wait = [&](const std::chrono::nanoseconds duration)
		{
			if (stop.stop_requested())
			{
				return false;
			}
			if (!clock.waitUntil(clock.now() + duration, stop) || stop.stop_requested())
			{
				return false;
			}
			if (duration > std::chrono::nanoseconds::zero())
			{
				consecutiveNoWaitSteps = 0;
			}
			return true;
		};
		const auto stroke = [&](const Stroke &inputStroke, std::chrono::nanoseconds press)
		{
			if (!wait(std::chrono::nanoseconds::zero()))
			{
				return false;
			}
			if (press == std::chrono::nanoseconds::zero())
			{
				auto batch = inputStroke.down;
				batch.insert(batch.end(), inputStroke.up.begin(), inputStroke.up.end());
				backend.send(batch);
			}
			else
			{
				backend.send(inputStroke.down);
				if (!wait(press))
				{
					return false;
				}
				backend.send(inputStroke.up);
			}
			++result.completedActions;
			return true;
		};
		const auto targetStroke = [&](const InputTarget &target)
		{
			if (target.kind == InputTarget::Kind::Key)
			{
				return backend.key(target.code);
			}
			const KeyboardEvent down{.code = target.code, .kind = KeyboardEvent::Kind::MouseButton};
			auto				up = down;
			up.up				   = true;
			return Stroke{.down = {down}, .up = {up}};
		};
		try
		{
			if (effectiveLimits)
			{
				validateScriptProgram(program, *effectiveLimits);
			}
			struct Loop
			{
				std::size_t					 begin;
				std::optional<std::uint64_t> remaining;
			};
			std::vector<Loop>		 loops;
			std::vector<InputTarget> heldTargets;
			for (std::size_t pc = 0; pc < program.nodes().size(); ++pc)
			{
				const auto &node = program.nodes()[pc];
				if (stop.stop_requested())
				{
					result.cancelled = true;
					break;
				}
				accountStep(std::holds_alternative<Delay>(node));
				if (const auto *text = std::get_if<Text>(&node))
				{
					for (std::size_t i = 0; i < text->value.size(); ++i)
					{
						if (stop.stop_requested() || (i && !wait(text->options.interval)))
						{
							result.cancelled = true;
							break;
						}
						accountStep();
						if (!stroke(backend.character(text->value[i], text->options.strategy), text->options.press))
						{
							result.cancelled = true;
							break;
						}
					}
				}
				else if (const auto *action = std::get_if<StrokeAction>(&node))
				{
					Stroke				combined;
					std::vector<Stroke> parts;
					parts.reserve(action->targets.size());
					for (const auto &target: action->targets)
					{
						if (std::ranges::find(heldTargets, target) != heldTargets.end())
						{
							throw InputError("[[execution.target_held]]");
						}
						parts.push_back(targetStroke(target));
						combined.down.insert(combined.down.end(), parts.back().down.begin(), parts.back().down.end());
					}
					for (auto part = parts.rbegin(); part != parts.rend(); ++part)
					{
						combined.up.insert(combined.up.end(), part->up.begin(), part->up.end());
					}
					result.cancelled = !stroke(combined, action->press);
				}
				else if (const auto *action = std::get_if<EventAction>(&node))
				{
					const auto found = std::ranges::find(heldTargets, action->target);
					if (action->up ? found == heldTargets.end() : found != heldTargets.end())
					{
						throw InputError("[[execution.unpaired_event]]");
					}
					const auto	inputStroke = targetStroke(action->target);
					const auto &events		= action->up ? inputStroke.up : inputStroke.down;
					if (events.size() != 1)
					{
						throw InputError("[[execution.atomic_input_required]]");
					}
					backend.send(events);
					if (action->up)
					{
						heldTargets.erase(found);
					}
					else
					{
						heldTargets.push_back(action->target);
					}
					++result.completedActions;
				}
				else if (const auto *delay = std::get_if<Delay>(&node))
				{
					result.cancelled = !wait(delay->duration);
				}
				else if (const auto *move = std::get_if<MouseMove>(&node))
				{
					const KeyboardEvent event{.kind = KeyboardEvent::Kind::MouseMove, .x = move->x, .y = move->y};
					backend.send(std::span<const KeyboardEvent>(&event, 1));
					++result.completedActions;
				}
				else if (const auto *wheel = std::get_if<MouseWheel>(&node))
				{
					const KeyboardEvent event{.kind = KeyboardEvent::Kind::Wheel, .y = wheel->steps};
					backend.send(std::span<const KeyboardEvent>(&event, 1));
					++result.completedActions;
				}
				else if (const auto *begin = std::get_if<RepeatBegin>(&node))
				{
					loops.push_back({.begin = pc, .remaining = begin->count});
				}
				else if (std::holds_alternative<RepeatEnd>(node))
				{
					auto &[loopBegin, remaining] = loops.back();
					if (!remaining || --*remaining)
					{
						pc = loopBegin;
					}
					else
					{
						loops.pop_back();
					}
				}
				if (result.cancelled)
				{
					break;
				}
			}
			if (!result.cancelled && !heldTargets.empty())
			{
				throw InputError("[[execution.unreleased_input]]");
			}
		}
		catch (...)
		{
			if (!backend.releaseAll())
			{
				std::throw_with_nested(CleanupError("[[execution.failure_cleanup_incomplete]]"));
			}
			throw;
		}
		if (!backend.releaseAll())
		{
			throw CleanupError("[[execution.cleanup_incomplete]]");
		}
		return result;
	}

	ExecutionTask makeProgramExecutionTask(Program program, InputBackendFactory factory, const std::optional<ScriptExecutionLimits> scriptLimits)
	{
		if (!factory)
		{
			throw std::invalid_argument("[[execution.backend_required]]");
		}
		if (scriptLimits)
		{
			validateScriptProgram(program, *scriptLimits);
		}
		return [program = std::move(program), factory = std::move(factory), scriptLimits](const std::stop_token &stop)
		{
			if (stop.stop_requested())
			{
				return;
			}
			const auto backend = factory(stop);
			if (stop.stop_requested())
			{
				return;
			}
			if (!backend)
			{
				throw InputError("[[input.backend_unavailable]]");
			}
			SteadyExecutionClock clock;
			executeProgram(program, *backend, clock, stop, scriptLimits);
		};
	}
} // namespace flori_input
