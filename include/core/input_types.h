/**
 * @file input_types.h
 * @brief 定义触发器输入配置及其基础约束。
 */
#ifndef FLORI_INPUT_CORE_INPUT_TYPES_H
#define FLORI_INPUT_CORE_INPUT_TYPES_H

#include <chrono>
#include <cstdint>

namespace flori_input
{
	enum class InputKey : std::uint8_t
	{
		MouseLeft,
		MouseMiddle,
		MouseRight,
		Custom,
	};

	enum class InputAction : std::uint8_t
	{
		Repeat,
		Hold,
	};

	enum class CursorMode : std::uint8_t
	{
		Free,
		Locked,
	};

	struct ScreenPoint
	{
		int x{};
		int y{};

		friend bool operator==(const ScreenPoint &, const ScreenPoint &) = default;
	};

	struct InjectorConfig
	{
		InputKey				 inputKey{InputKey::MouseLeft};
		InputAction				 action{InputAction::Repeat};
		CursorMode				 cursorMode{CursorMode::Free};
		std::uint32_t			 customKey{};
		ScreenPoint				 lockedCoordinate{};
		std::chrono::nanoseconds cycle{std::chrono::milliseconds(10)};
		// 零表示原子提交按下与释放；正值表示保持按下的时长。
		std::chrono::nanoseconds pressDuration{std::chrono::milliseconds(5)};
		double					 timingJitterPercent{};
	};

	/** @brief 三个输入页的新配置共用的默认启动热键（F8）。 */
	inline constexpr std::uint32_t DefaultStartHotkey		  = 0x77;
	constexpr std::uint32_t		   WheelUpKey				  = 0x10001;
	constexpr std::uint32_t		   WheelDownKey				  = 0x10002;
	constexpr auto				   MinimumInputCycle		  = std::chrono::milliseconds(1);
	constexpr auto				   MinimumReleaseDuration	  = std::chrono::milliseconds(1);
	constexpr double			   MaximumTimingJitterPercent = 50.0;
	// 为抖动及 steady_clock 截止时间计算留出余量。
	constexpr auto MaximumInputCycle	= std::chrono::nanoseconds::max() / 4;
	constexpr auto MaximumPressDuration = MaximumInputCycle - MinimumReleaseDuration;

	/** @brief 判断自定义输入是否为滚轮方向。 */
	constexpr bool isWheelKey(std::uint32_t key)
	{
		return key == WheelUpKey || key == WheelDownKey;
	}

	/** @brief 判断当前触发器配置是否支持独立按压时间。 */
	constexpr bool supportsPressDuration(const InjectorConfig &config)
	{
		return config.action == InputAction::Repeat && !(config.inputKey == InputKey::Custom && isWheelKey(config.customKey));
	}

	/** @brief 判断当前配置是否需要分开发送按下和抬起事件。 */
	constexpr bool usesTimedPress(const InjectorConfig &config)
	{
		return supportsPressDuration(config) && config.pressDuration > std::chrono::nanoseconds::zero();
	}

	/** @brief 计算按压时间约束下允许的最小输入周期。 */
	constexpr std::chrono::nanoseconds minimumInputCycle(const InjectorConfig &config)
	{
		if (!usesTimedPress(config))
		{
			return MinimumInputCycle;
		}
		return config.pressDuration + MinimumReleaseDuration;
	}
} // namespace flori_input

#endif // FLORI_INPUT_CORE_INPUT_TYPES_H
