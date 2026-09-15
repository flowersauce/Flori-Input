/**
 * @file script_execution_limits.h
 * @brief 定义事件剧本专用的执行步数保护策略。
 */
#ifndef FLORI_INPUT_CORE_SCRIPT_EXECUTION_LIMITS_H
#define FLORI_INPUT_CORE_SCRIPT_EXECUTION_LIMITS_H

#include <cstdint>

namespace flori_input
{
	class Program;

	inline constexpr std::uint32_t MaximumConfiguredFiniteSteps			   = 1'000'000;
	inline constexpr std::uint32_t MaximumConfiguredConsecutiveNoWaitSteps = 10'000;

	struct ScriptExecutionLimits
	{
		std::uint32_t maxFiniteSteps{100'000};
		std::uint32_t maxConsecutiveNoWaitSteps{1'000};
		friend bool	  operator==(const ScriptExecutionLimits &, const ScriptExecutionLimits &) = default;
	};
	inline constexpr ScriptExecutionLimits DefaultScriptExecutionLimits{};

	/** @brief 将无效的配置字段分别恢复为默认值。 */
	constexpr ScriptExecutionLimits normalizedScriptExecutionLimits(ScriptExecutionLimits limits) noexcept
	{
		if (!limits.maxFiniteSteps || limits.maxFiniteSteps > MaximumConfiguredFiniteSteps)
		{
			limits.maxFiniteSteps = DefaultScriptExecutionLimits.maxFiniteSteps;
		}
		if (!limits.maxConsecutiveNoWaitSteps || limits.maxConsecutiveNoWaitSteps > MaximumConfiguredConsecutiveNoWaitSteps)
		{
			limits.maxConsecutiveNoWaitSteps = DefaultScriptExecutionLimits.maxConsecutiveNoWaitSteps;
		}
		return limits;
	}

	/** @brief 校验可达路径的按键配对、有限总步数及跨循环的连续无等待步数。 */
	void validateScriptProgram(const Program &program, const ScriptExecutionLimits &limits);
} // namespace flori_input

#endif // FLORI_INPUT_CORE_SCRIPT_EXECUTION_LIMITS_H
