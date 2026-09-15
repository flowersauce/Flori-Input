/**
 * @file input_program.cpp
 * @brief 实现输入程序校验、文本解码和选项应用。
 */
#include "core/input_program.h"
#include "core/script_execution_limits.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace flori_input
{
	Program::Program(std::vector<ProgramNode> nodes)
		: nodes_(std::move(nodes))
	{
		const auto validateTime = [](const std::chrono::nanoseconds value)
		{
			if (value < std::chrono::nanoseconds::zero() || value > std::chrono::hours(24))
			{
				throw std::invalid_argument("[[program.time_range]]");
			}
		};
		const auto validateTarget = [](const InputTarget &target)
		{
			if ((target.kind == InputTarget::Kind::Key && (target.code == 0 || target.code > 0xfe))
				|| (target.kind == InputTarget::Kind::Mouse && target.code > static_cast<std::uint16_t>(MouseButton::Right))
				|| (target.kind != InputTarget::Kind::Key && target.kind != InputTarget::Kind::Mouse))
			{
				throw std::invalid_argument("[[program.invalid_target]]");
			}
		};
		std::vector<std::size_t> loops;
		std::vector<bool>		 advances;
		for (std::size_t index = 0; index < nodes_.size(); ++index)
		{
			const auto &node	 = nodes_[index];
			bool		progress = false;
			if (const auto *begin = std::get_if<RepeatBegin>(&node))
			{
				if (begin->end <= index + 1 || begin->end >= nodes_.size() || loops.size() >= 16
					|| (begin->count && (*begin->count == 0 || *begin->count > MaximumRepeatCount)))
				{
					throw std::invalid_argument("[[program.invalid_repeat]]");
				}
				loops.push_back(index);
				advances.push_back(false);
			}
			if (const auto *end = std::get_if<RepeatEnd>(&node))
			{
				if (loops.empty() || loops.back() != end->begin || std::get<RepeatBegin>(nodes_[end->begin]).end != index)
				{
					throw std::invalid_argument("[[program.unmatched_repeat]]");
				}
				progress = advances.back();
				if (!std::get<RepeatBegin>(nodes_[end->begin]).count && !progress)
				{
					throw std::invalid_argument("[[program.infinite_without_wait]]");
				}
				loops.pop_back();
				advances.pop_back();
			}
			if (const auto *text = std::get_if<Text>(&node))
			{
				validateTime(text->options.press);
				validateTime(text->options.interval);
				progress = (!text->value.empty() && text->options.press.count() > 0) || (text->value.size() > 1 && text->options.interval.count() > 0);
				for (const char32_t ch: text->value)
				{
					if (ch > 0x10ffff || (ch >= 0xd800 && ch <= 0xdfff))
					{
						throw std::invalid_argument("[[program.invalid_unicode]]");
					}
				}
			}
			if (const auto *stroke = std::get_if<StrokeAction>(&node))
			{
				validateTime(stroke->press);
				progress = stroke->press.count() > 0;
				if (stroke->targets.empty() || stroke->targets.size() > MaximumStrokeTargets)
				{
					throw std::invalid_argument("[[program.invalid_target_count]]");
				}
				for (std::size_t i = 0; i < stroke->targets.size(); ++i)
				{
					validateTarget(stroke->targets[i]);
					if (std::find(stroke->targets.begin(), stroke->targets.begin() + i, stroke->targets[i]) != stroke->targets.begin() + i)
					{
						throw std::invalid_argument("[[program.duplicate_target]]");
					}
				}
			}
			if (const auto *event = std::get_if<EventAction>(&node))
			{
				validateTarget(event->target);
			}
			if (const auto *delay = std::get_if<Delay>(&node);
				delay && (delay->duration < std::chrono::nanoseconds::zero() || delay->duration > std::chrono::hours(24)))
			{
				throw std::invalid_argument("[[program.delay_range]]");
			}
			if (const auto *delay = std::get_if<Delay>(&node))
			{
				progress = delay->duration.count() > 0;
			}
			if (const auto *wheel = std::get_if<MouseWheel>(&node);
				wheel && (wheel->steps == 0 || wheel->steps < -MaximumWheelSteps || wheel->steps > MaximumWheelSteps))
			{
				throw std::invalid_argument("[[program.wheel_range]]");
			}
			if (!advances.empty() && progress)
			{
				advances.back() = true;
			}
		}
		if (!loops.empty())
		{
			throw std::invalid_argument("[[program.unclosed_repeat]]");
		}
	}

	void validateScriptProgram(const Program &program, const ScriptExecutionLimits &limits)
	{
		const auto effective = normalizedScriptExecutionLimits(limits);
		const auto nodes = program.nodes();
		const auto cap = static_cast<std::uint64_t>(std::max(effective.maxFiniteSteps, effective.maxConsecutiveNoWaitSteps));
		const auto add = [cap](const std::uint64_t a, const std::uint64_t b) { return std::min(cap + 1, a + b); };
		/** @brief 记录总步数及等待前后、跨指令边界的连续无等待步数。 */
		struct Steps
		{
			std::uint64_t total{}, prefix{}, suffix{}, peak{};
			bool waits{};
		};
		const auto join = [&](const Steps &a, const Steps &b)
		{
			return Steps{.total = add(a.total, b.total),
				.prefix = a.waits ? a.prefix : add(a.prefix, b.prefix),
				.suffix = b.waits ? b.suffix : add(a.suffix, b.suffix),
				.peak = std::max({a.peak, b.peak, add(a.suffix, b.prefix)}),
				.waits = a.waits || b.waits};
		};
		// 二进制合并摘要，无需按循环次数展开节点。
		const auto repeatSteps = [&](Steps body, std::uint64_t count)
		{
			Steps result;
			while (count)
			{
				if (count & 1)
				{
					result = join(result, body);
				}
				count >>= 1;
				body = join(body, body);
			}
			return result;
		};
		/** @brief 每个被访问目标的入口状态要求与出口状态。 */
		struct TargetState
		{
			bool required;
			bool held;
			const char *error;
		};
		using TargetId = std::pair<InputTarget::Kind, std::uint16_t>;
		/** @brief 摘要覆盖实际可达路径；无限循环之后的节点不可达。 */
		struct Summary
		{
			Steps steps;
			std::map<TargetId, TargetState> targets;
			bool returns{true};
		};
		const auto mergeTargets = [](std::map<TargetId, TargetState> &destination, const std::map<TargetId, TargetState> &source)
		{
			for (const auto &[target, state]: source)
			{
				const auto [found, inserted] = destination.emplace(target, state);
				if (!inserted)
				{
					if (found->second.held != state.required)
					{
						throw std::invalid_argument(state.error);
					}
					found->second.held = state.held;
				}
			}
		};
		const Steps step{.total = 1, .prefix = 1, .suffix = 1, .peak = 1};
		const Steps reset{.waits = true};
		const auto summarize = [&](const auto &self, const std::size_t first, const std::size_t last) -> Summary
		{
			Summary result;
			for (auto index = first; index < last && result.returns; ++index)
			{
				const auto &node = nodes[index];
				Summary current;
				current.steps = step;
				if (const auto *begin = std::get_if<RepeatBegin>(&node))
				{
					current = self(self, index + 1, begin->end);
					if (current.returns)
					{
						if (!begin->count || *begin->count > 1)
						{
							for (const auto &[target, state]: current.targets)
							{
								if (state.held != state.required)
								{
									throw std::invalid_argument(state.error);
								}
							}
						}
						// RepeatBegin 只执行一次，每轮执行体及 RepeatEnd。
						const auto iteration = join(current.steps, step);
						// 含等待的无限循环只需两轮即可覆盖轮内及跨轮的最长无等待段。
						current.steps = repeatSteps(iteration, begin->count.value_or(2));
						current.returns = begin->count.has_value();
					}
					current.steps = join(step, current.steps);
					index = begin->end;
				}
				else if (const auto *delay = std::get_if<Delay>(&node))
				{
					if (delay->duration.count() == 0)
					{
						throw std::invalid_argument("[[script.positive_wait]]");
					}
					current.steps = Steps{.total = 1, .waits = true};
				}
				else if (const auto *text = std::get_if<Text>(&node); text && !text->value.empty())
				{
					const auto character = text->options.press.count() > 0 ? join(step, reset) : step;
					const auto next = text->options.interval.count() > 0 ? join(reset, character) : character;
					current.steps = join(step, join(character, repeatSteps(next, text->value.size() - 1)));
				}
				else if (const auto *stroke = std::get_if<StrokeAction>(&node))
				{
					for (const auto &target: stroke->targets)
					{
						current.targets.emplace(TargetId{target.kind, target.code}, TargetState{false, false, "[[execution.target_held]]"});
					}
					if (stroke->press.count() > 0)
					{
						current.steps = join(step, reset);
					}
				}
				else if (const auto *event = std::get_if<EventAction>(&node))
				{
					current.targets.emplace(TargetId{event->target.kind, event->target.code},
						TargetState{event->up, !event->up, "[[execution.unpaired_event]]"});
				}
				result.steps = join(result.steps, current.steps);
				mergeTargets(result.targets, current.targets);
				result.returns = current.returns;
			}
			return result;
		};
		const auto summary = summarize(summarize, 0, nodes.size());
		for (const auto &[target, state]: summary.targets)
		{
			if (state.required)
			{
				throw std::invalid_argument(state.error);
			}
			if (summary.returns && state.held)
			{
				throw std::invalid_argument("[[execution.unreleased_input]]");
			}
		}
		if (summary.returns && summary.steps.total > effective.maxFiniteSteps)
		{
			throw std::invalid_argument("[[program.step_limit]]");
		}
		if (summary.steps.peak > effective.maxConsecutiveNoWaitSteps)
		{
			throw std::invalid_argument("[[execution.no_wait_limit]]");
		}
	}
	Program withInputOptions(const Program &program, const InputOptions &options)
	{
		std::vector<ProgramNode> nodes(program.nodes().begin(), program.nodes().end());
		for (auto &node: nodes)
		{
			if (auto *text = std::get_if<Text>(&node))
			{
				text->options = options;
			}
			else if (auto *stroke = std::get_if<StrokeAction>(&node))
			{
				stroke->press = options.press;
			}
		}
		return Program(std::move(nodes));
	}

	Program plainTextProgram(const std::u16string_view text)
	{
		std::u32string scalars;
		for (std::size_t i = 0; i < text.size(); ++i)
		{
			char32_t ch = text[i];
			if (ch >= 0xd800 && ch <= 0xdbff)
			{
				if (++i == text.size() || text[i] < 0xdc00 || text[i] > 0xdfff)
				{
					throw std::invalid_argument("[[encoding.unpaired_surrogate]]");
				}
				ch = 0x10000 + ((ch - 0xd800) << 10) + (text[i] - 0xdc00);
			}
			else if (ch >= 0xdc00 && ch <= 0xdfff)
			{
				throw std::invalid_argument("[[encoding.unpaired_surrogate]]");
			}
			if (ch == U'\r')
			{
				if (i + 1 < text.size() && text[i + 1] == u'\n')
				{
					++i;
				}
				ch = U'\n';
			}
			scalars.push_back(ch);
		}
		return Program({Text{.value = std::move(scalars)}});
	}

	Program plainTextProgramUtf8(const std::string_view text)
	{
		std::u16string utf16;
		for (std::size_t i = 0; i < text.size();)
		{
			const auto first = static_cast<unsigned char>(text[i++]);
			char32_t   ch{};
			int		   trailing{};
			char32_t   minimum{};
			if (first < 0x80)
			{
				ch = first;
			}
			else if (first >= 0xc2 && first <= 0xdf)
			{
				ch		 = first & 0x1f;
				trailing = 1;
				minimum	 = 0x80;
			}
			else if (first >= 0xe0 && first <= 0xef)
			{
				ch		 = first & 0x0f;
				trailing = 2;
				minimum	 = 0x800;
			}
			else if (first >= 0xf0 && first <= 0xf4)
			{
				ch		 = first & 0x07;
				trailing = 3;
				minimum	 = 0x10000;
			}
			else
			{
				throw std::invalid_argument("[[encoding.invalid_utf8_lead]]");
			}
			for (int n = 0; n < trailing; ++n)
			{
				if (i == text.size())
				{
					throw std::invalid_argument("[[encoding.truncated_utf8]]");
				}
				const auto next = static_cast<unsigned char>(text[i++]);
				if ((next & 0xc0) != 0x80)
				{
					throw std::invalid_argument("[[encoding.invalid_utf8_continuation]]");
				}
				ch = (ch << 6) | (next & 0x3f);
			}
			if (ch < minimum || ch > 0x10ffff || (ch >= 0xd800 && ch <= 0xdfff))
			{
				throw std::invalid_argument("[[encoding.invalid_utf8_scalar]]");
			}
			if (ch <= 0xffff)
			{
				utf16.push_back(static_cast<char16_t>(ch));
			}
			else
			{
				ch -= 0x10000;
				utf16.push_back(static_cast<char16_t>(0xd800 + (ch >> 10)));
				utf16.push_back(static_cast<char16_t>(0xdc00 + (ch & 0x3ff)));
			}
		}
		return plainTextProgram(utf16);
	}
} // namespace flori_input
