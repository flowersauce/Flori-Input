/**
 * @file input_program.h
 * @brief 定义模拟输入程序的不可变中间表示。
 */
#ifndef FLORI_INPUT_CORE_INPUT_PROGRAM_H
#define FLORI_INPUT_CORE_INPUT_PROGRAM_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace flori_input
{
	inline constexpr std::uint64_t MaximumRepeatCount = 1'000'000'000;
	/** @brief 单条滚轮指令允许的最大步数绝对值。 */
	inline constexpr int MaximumWheelSteps = 100;
	/** @brief 单条复合动作允许的最大输入目标数量。 */
	inline constexpr std::size_t MaximumStrokeTargets = 32;

	enum class TextInputStrategy
	{
		PreferPhysical,
		Unicode
	};
	struct InputOptions
	{
		TextInputStrategy		 strategy{TextInputStrategy::PreferPhysical};
		std::chrono::nanoseconds press{std::chrono::milliseconds(5)};
		std::chrono::nanoseconds interval{std::chrono::milliseconds(10)};
	};
	struct Text
	{
		std::u32string value;
		InputOptions   options;
	};
	struct Delay
	{
		std::chrono::nanoseconds duration;
	};
	/** @brief 剧本支持的鼠标按键，数值对应平台中立事件编号。 */
	enum class MouseButton : std::uint16_t
	{
		Left,
		Middle,
		Right
	};
	/** @brief 键盘虚拟键或鼠标按键组成的剧本输入目标。 */
	struct InputTarget
	{
		enum class Kind
		{
			Key,
			Mouse
		};
		Kind		  kind;
		std::uint16_t code;
		friend bool	  operator==(const InputTarget &, const InputTarget &) = default;
	};
	/** @brief 按顺序按下目标并逆序释放的复合动作。 */
	struct StrokeAction
	{
		std::vector<InputTarget> targets;
		std::chrono::nanoseconds press;
	};
	/** @brief 单个目标的一次按下或抬起。 */
	struct EventAction
	{
		InputTarget target;
		bool		up;
	};
	/** @brief 虚拟桌面物理像素中的绝对鼠标位置。 */
	struct MouseMove
	{
		int			x;
		int			y;
		std::size_t line{};
		std::size_t column{};
	};
	/** @brief 在当前指针位置滚动指定有符号步数。 */
	struct MouseWheel
	{
		int steps;
	};
	struct RepeatBegin
	{
		std::optional<std::uint64_t> count;
		std::size_t					 end;
	}; // 空计数表示无限循环。
	struct RepeatEnd
	{
		std::size_t begin;
	};
	using ProgramNode = std::variant<Text, StrokeAction, EventAction, Delay, MouseMove, MouseWheel, RepeatBegin, RepeatEnd>;

	// 持有已校验的快照，不暴露可变节点访问。
	class Program final
	{
	public:
		/** @brief 校验并接管已生成的节点序列。 */
		explicit Program(std::vector<ProgramNode> nodes);
		/** @brief 以只读视图访问冻结后的执行节点。 */
		std::span<const ProgramNode> nodes() const
		{
			return nodes_;
		}

	private:
		std::vector<ProgramNode> nodes_;
	};

	/** @brief 将 UTF-16 纯文本转换为输入程序。 */
	Program plainTextProgram(std::u16string_view text);
	/** @brief 将 UTF-8 纯文本转换为输入程序。 */
	Program plainTextProgramUtf8(std::string_view text);
	// 配置构造器在执行前将页面设置固化为程序节点。
	/** @brief 将界面输入策略和时序参数固化到程序节点。 */
	Program withInputOptions(const Program &program, const InputOptions &options);
} // namespace flori_input

#endif // FLORI_INPUT_CORE_INPUT_PROGRAM_H
