/**
 * @file event_script.cpp
 * @brief 实现事件剧本语法解析与输入程序生成。
 */
#include "core/event_script.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace flori_input
{
	namespace
	{
		using View = std::u32string_view;
		/** @brief 判断码点是否为 ASCII 数字。 */
		bool digit(const char32_t c)
		{
			return c >= U'0' && c <= U'9';
		}
		/** @brief 判断码点是否为 ASCII 字母。 */
		bool alpha(const char32_t c)
		{
			return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
		}
		class Parser
		{
		public:
			/** @brief 绑定已解码的剧本文本视图。 */
			explicit Parser(const View source)
				: source_(source)
			{
				for (std::size_t i = 0; i < source_.size(); ++i)
				{
					if (source_[i] == U'\n')
					{
						lineStarts_.push_back(i + 1);
					}
				}
			}
			/** @brief 解析完整剧本并构造已验证程序。 */
			Program parse()
			{
				block(false, 0);
				try
				{
					return Program(std::move(nodes_));
				}
				catch (const std::invalid_argument &)
				{
					fail("[[script.invalid_program]]");
				}
			}

		private:
			View					 source_;
			std::size_t				 pos_{};
			std::vector<std::size_t> lineStarts_{0};
			std::vector<ProgramNode> nodes_;
			/** @brief 将源码偏移换算为从一开始的行列号。 */
			std::pair<std::size_t, std::size_t> location(const std::size_t position) const
			{
				const auto nextLine = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), position);
				return {static_cast<std::size_t>(nextLine - lineStarts_.begin()), position - *(nextLine - 1) + 1};
			}
			/** @brief 依据当前游标抛出带行列号的解析错误。 */
			[[noreturn]] void fail(const char *message) const
			{
				const auto [line, column] = location(pos_);
				throw EventScriptError(line, column, message);
			}
			/** @brief 跳过空白和行注释。 */
			void space()
			{
				while (pos_ < source_.size())
				{
					if (const auto c = source_[pos_]; c == U' ' || c == U'\t' || c == U'\n' || c == U'\r')
					{
						++pos_;
						continue;
					}
					if (source_.substr(pos_, 2) == U"//")
					{
						while (pos_ < source_.size() && source_[pos_] != U'\n')
						{
							++pos_;
						}
						continue;
					}
					break;
				}
			}
			/** @brief 忽略前导空白后尝试消费指定分隔符。 */
			bool take(const char32_t c)
			{
				space();
				if (pos_ < source_.size() && source_[pos_] == c)
				{
					++pos_;
					return true;
				}
				return false;
			}
			/** @brief 要求当前位置出现指定分隔符。 */
			void expect(const char32_t c)
			{
				if (!take(c))
				{
					fail("[[script.expected_delimiter]]");
				}
			}
			/** @brief 读取标识符或数值参数 token。 */
			View token()
			{
				space();
				const auto begin = pos_;
				if (pos_ < source_.size() && source_[pos_] == U'-')
				{
					++pos_;
				}
				while (pos_ < source_.size() && (alpha(source_[pos_]) || digit(source_[pos_]) || source_[pos_] == U'.' || source_[pos_] == U'_'))
				{
					++pos_;
				}
				if (begin == pos_)
				{
					fail("[[script.expected_argument]]");
				}
				return source_.substr(begin, pos_ - begin);
			}
			/** @brief 将整数毫秒解析为纳秒时长。 */
			std::chrono::nanoseconds duration(const View value) const
			{
				constexpr std::uint64_t MaxMilliseconds			  = 86'400'000;
				constexpr std::uint64_t NanosecondsPerMillisecond = 1'000'000;
				std::uint64_t			milliseconds			  = 0;
				for (const auto c: value)
				{
					if (!digit(c))
					{
						fail("[[script.integer_milliseconds]]");
					}
					const auto next = static_cast<std::uint64_t>(c - U'0');
					if (milliseconds > MaxMilliseconds / 10 || (milliseconds == MaxMilliseconds / 10 && next > MaxMilliseconds % 10))
					{
						fail("[[script.time_range]]");
					}
					milliseconds = milliseconds * 10 + next;
				}
				if (value.empty())
				{
					fail("[[script.expected_milliseconds]]");
				}
				return std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(milliseconds * NanosecondsPerMillisecond));
			}
			/** @brief 按指定范围解析有符号整数，避免中间计算溢出。 */
			int integer(const View value, const int minimum, const int maximum) const
			{
				const bool negative = !value.empty() && value.front() == U'-';
				const auto digits	= value.substr(negative ? 1 : 0);
				if (digits.empty())
				{
					fail("[[script.expected_integer]]");
				}
				const auto	  limit	 = negative ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(minimum)) : static_cast<std::uint64_t>(maximum);
				std::uint64_t number = 0;
				for (const auto digitValue: digits)
				{
					if (!digit(digitValue))
					{
						fail("[[script.expected_integer]]");
					}
					const auto next = static_cast<std::uint64_t>(digitValue - U'0');
					if (number > limit / 10 || (number == limit / 10 && next > limit % 10))
					{
						fail("[[script.integer_range]]");
					}
					number = number * 10 + next;
				}
				return static_cast<int>(negative ? -static_cast<std::int64_t>(number) : static_cast<std::int64_t>(number));
			}
			/** @brief 解析剧本中支持的鼠标按键名称。 */
			MouseButton mouseButton(const View name) const
			{
				if (name == U"Left")
				{
					return MouseButton::Left;
				}
				if (name == U"Middle")
				{
					return MouseButton::Middle;
				}
				if (name == U"Right")
				{
					return MouseButton::Right;
				}
				fail("[[script.unknown_button]]");
			}
			/** @brief 解析显式限定命名空间的键盘或鼠标输入目标。 */
			InputTarget target(const View name) const
			{
				if (name.starts_with(U"Key."))
				{
					return {.kind = InputTarget::Kind::Key, .code = key(name.substr(4))};
				}
				if (name.starts_with(U"Mouse."))
				{
					return {.kind = InputTarget::Kind::Mouse, .code = static_cast<std::uint16_t>(mouseButton(name.substr(6)))};
				}
				fail("[[script.expected_target]]");
			}
			/** @brief 将剧本中的按键名称映射为虚拟键码。 */
			std::uint16_t key(const View name) const
			{
				if (name.size() == 1 && ((name[0] >= U'A' && name[0] <= U'Z') || digit(name[0])))
				{
					return static_cast<std::uint16_t>(name[0]);
				}
				if (name.size() >= 2 && name.size() <= 3 && name[0] == U'F' && name[1] >= U'1' && name[1] <= U'9')
				{
					if (const auto n = name.size() == 2 ? name[1] - U'0' : digit(name[2]) ? (name[1] - U'0') * 10 + name[2] - U'0' : 0; n >= 1 && n <= 24)
					{
						return static_cast<std::uint16_t>(0x70 + n - 1);
					}
				}
				constexpr std::pair<View, std::uint16_t> names[]{{U"Ctrl", 0xa2},
																 {U"Shift", 0xa0},
																 {U"Alt", 0xa4},
																 {U"Win", 0x5b},
																 {U"Enter", 13},
																 {U"Tab", 9},
																 {U"Esc", 27},
																 {U"Space", 32},
																 {U"Backspace", 8},
																 {U"Delete", 46},
																 {U"Insert", 45},
																 {U"Home", 36},
																 {U"End", 35},
																 {U"PageUp", 33},
																 {U"PageDown", 34},
																 {U"ArrowLeft", 37},
																 {U"ArrowRight", 39},
																 {U"ArrowUp", 38},
																 {U"ArrowDown", 40}};
				for (auto [canonical, code]: names)
				{
					if (name == canonical)
					{
						return code;
					}
				}
				fail("[[script.unknown_key]]");
			}
			/** @brief 读取纯 Unicode 字符串；仅双写双引号表示字面引号。 */
			std::u32string string()
			{
				expect(U'"');
				std::u32string result;
				while (pos_ < source_.size())
				{
					const auto c = source_[pos_++];
					if (c == U'"')
					{
						if (pos_ < source_.size() && source_[pos_] == U'"')
						{
							++pos_;
							result.push_back(U'"');
							continue;
						}
						return result;
					}
					if (c == U'\n' || c == U'\r')
					{
						fail("[[script.unclosed_string]]");
					}
					if (c < U' ' || (c >= 0x7f && c <= 0x9f))
					{
						fail("[[script.control_character]]");
					}
					result.push_back(c);
				}
				fail("[[script.unclosed_string]]");
			}
			/** @brief 递归解析指令块并报告是否包含时间推进。 */
			bool block(const bool nested, const unsigned depth)
			{
				bool progress = false;
				for (;;)
				{
					space();
					if (pos_ == source_.size())
					{
						if (nested)
						{
							fail("[[script.missing_brace]]");
						}
						return progress;
					}
					if (take(U'}'))
					{
						if (!nested)
						{
							fail("[[script.unexpected_brace]]");
						}
						return progress;
					}
					const auto commandPosition = pos_;
					const auto name			   = token();
					expect(U'(');
					if (name == U"text")
					{
						auto value = string();
						expect(U',');
						const auto press = duration(token());
						expect(U',');
						const auto interval = duration(token());
						expect(U')');
						progress |= (!value.empty() && press.count() > 0) || (value.size() > 1 && interval.count() > 0);
						nodes_.push_back(
							Text{.value = std::move(value), .options = {.strategy = TextInputStrategy::Unicode, .press = press, .interval = interval}});
					}
					else if (name == U"stroke")
					{
						std::vector<View> args;
						do
						{
							args.push_back(token());
						} while (take(U','));
						expect(U')');
						if (args.size() < 2)
						{
							fail("[[script.stroke_arguments]]");
						}
						const auto press = duration(args.back());
						args.pop_back();
						if (args.size() > MaximumStrokeTargets)
						{
							fail("[[script.stroke_target_limit]]");
						}
						std::vector<InputTarget> targets;
						for (const auto arg: args)
						{
							const auto inputTarget = target(arg);
							if (std::ranges::find(targets, inputTarget) != targets.end())
							{
								fail("[[script.duplicate_target]]");
							}
							targets.push_back(inputTarget);
						}
						nodes_.push_back(StrokeAction{.targets = std::move(targets), .press = press});
						progress |= press.count() > 0;
					}
					else if (name == U"event")
					{
						const auto inputTarget = target(token());
						expect(U',');
						const auto transition = token();
						if (transition != U"Down" && transition != U"Up")
						{
							fail("[[script.expected_direction]]");
						}
						expect(U')');
						nodes_.push_back(EventAction{.target = inputTarget, .up = transition == U"Up"});
					}
					else if (name == U"wait")
					{
						const auto value = duration(token());
						expect(U')');
						if (value == std::chrono::nanoseconds::zero())
						{
							fail("[[script.positive_wait]]");
						}
						nodes_.push_back(Delay{value});
						progress |= value.count() > 0;
					}
					else if (name == U"move")
					{
						const int x = integer(token(), std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
						expect(U',');
						const int y = integer(token(), std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
						expect(U')');
						const auto [line, column] = location(commandPosition);
						nodes_.push_back(MouseMove{.x = x, .y = y, .line = line, .column = column});
					}
					else if (name == U"wheel")
					{
						const int steps = integer(token(), -MaximumWheelSteps, MaximumWheelSteps);
						expect(U')');
						if (steps == 0)
						{
							fail("[[script.zero_wheel_steps]]");
						}
						nodes_.push_back(MouseWheel{.steps = steps});
					}
					else if (name == U"repeat")
					{
						const auto					 value = token();
						std::optional<std::uint64_t> count;
						if (value != U"-1")
						{
							std::uint64_t finiteCount = 0;
							for (const auto c: value)
							{
								if (!digit(c) || finiteCount > MaximumRepeatCount / 10)
								{
									fail("[[script.invalid_repeat]]");
								}
								finiteCount = finiteCount * 10 + c - U'0';
							}
							if (!finiteCount || finiteCount > MaximumRepeatCount)
							{
								fail("[[script.invalid_repeat]]");
							}
							count = finiteCount;
						}
						expect(U')');
						expect(U'{');
						if (depth >= 16)
						{
							fail("[[script.repeat_depth]]");
						}
						const auto begin = nodes_.size();
						nodes_.push_back(RepeatBegin{.count = count, .end = 0});
						const bool advances = block(true, depth + 1);
						if (nodes_.size() == begin + 1)
						{
							fail("[[script.empty_repeat]]");
						}
						if (!count && !advances)
						{
							fail("[[script.infinite_without_wait]]");
						}
						std::get<RepeatBegin>(nodes_[begin]).end = nodes_.size();
						nodes_.push_back(RepeatEnd{begin});
						progress |= advances;
						continue;
					}
					else
					{
						fail("[[script.unknown_command]]");
					}
					expect(U';');
				}
			}
		};
	} // namespace
	Program eventScriptProgram(std::string_view source)
	{
		if (source.starts_with("\xef\xbb\xbf"))
		{
			source.remove_prefix(3);
		}
		const auto decoded = plainTextProgramUtf8(source);
		return Parser(std::get<Text>(decoded.nodes()[0]).value).parse();
	}
} // namespace flori_input
