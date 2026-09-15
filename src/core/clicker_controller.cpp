/**
 * @file clicker_controller.cpp
 * @brief 实现触发器控制状态机和配置归一化。
 */
#include "core/clicker_controller.h"
#include <Windows.h>
#include <algorithm>
#include <stdexcept>
#include <utility>

#include "core/key_catalog.h"

namespace flori_input
{
	namespace
	{
		/** @brief 判断虚拟键是否为受支持的鼠标按键。 */
		bool isMouse(const std::uint32_t key)
		{
			return key == VK_LBUTTON || key == VK_MBUTTON || key == VK_RBUTTON;
		}
	} // namespace

	ClickerController::ClickerController(Start start, Stop stop)
		: startOperation(std::move(start))
		, stopOperation(std::move(stop))
	{
		if (!startOperation || !stopOperation)
		{
			throw std::invalid_argument("[[trigger.operations_required]]");
		}
	}

	bool ClickerController::canStart() const
	{
		return !running() && captureValue == Capture::None && (configValue.injector.inputKey != InputKey::Custom || configValue.injector.customKey != 0);
	}

	std::string_view ClickerController::statusText() const
	{
		if (running())
		{
			return "[[trigger.stop_hint]]";
		}
		if (invalidCaptureKey)
		{
			return "[[trigger.key_unavailable]]";
		}
		switch (captureValue)
		{
			case Capture::StartHotkey:
				return "[[trigger.capture_hotkey]]";
			case Capture::CustomKey:
				return "[[trigger.capture_key]]";
			case Capture::Coordinate:
			case Capture::CoordinateSelection:
				return "[[trigger.capture_point]]";
			case Capture::None:
				return canStart() ? "[[trigger.start_hint]]" : "[[trigger.setup_required]]";
		}
		return "[[trigger.setup_required]]";
	}

	bool ClickerController::setConfig(const Config &config)
	{
		if (running() || captureValue != Capture::None)
		{
			return false;
		}
		configValue = normalizedConfig(config);
		return true;
	}

	ClickerController::Config ClickerController::normalizedConfig(Config config)
	{
		auto &input = config.injector;
		if (!keyMap.contains(config.startHotkey))
		{
			config.startHotkey = DefaultStartHotkey;
		}
		if (!keyMap.contains(input.customKey))
		{
			input.customKey = 0;
		}
		input.inputKey			  = static_cast<InputKey>(std::clamp(static_cast<int>(input.inputKey), 0, 3));
		input.action			  = static_cast<InputAction>(std::clamp(static_cast<int>(input.action), 0, 1));
		input.cursorMode		  = static_cast<CursorMode>(std::clamp(static_cast<int>(input.cursorMode), 0, 1));
		input.pressDuration		  = std::clamp(input.pressDuration, std::chrono::nanoseconds::zero(), MaximumPressDuration);
		input.timingJitterPercent = std::isfinite(input.timingJitterPercent) ? std::clamp(input.timingJitterPercent, 0.0, MaximumTimingJitterPercent) : 0.0;
		if (input.customKey == config.startHotkey)
		{
			input.customKey = 0;
			if (input.inputKey == InputKey::Custom)
			{
				input.inputKey = InputKey::MouseLeft;
			}
		}
		if (input.inputKey == InputKey::Custom && isWheelKey(input.customKey))
		{
			input.action = InputAction::Repeat;
		}
		input.cycle = std::clamp(input.cycle, minimumInputCycle(input), MaximumInputCycle);
		return config;
	}

	void ClickerController::beginCapture(const Capture capture)
	{
		if (running())
		{
			return;
		}
		captureValue	  = capture;
		invalidCaptureKey = false;
		if (capture == Capture::CustomKey)
		{
			configValue.injector.inputKey = InputKey::Custom;
		}
		if (capture == Capture::Coordinate)
		{
			configValue.injector.cursorMode = CursorMode::Locked;
		}
	}

	void ClickerController::cancelCoordinateCapture()
	{
		if (captureValue == Capture::Coordinate || captureValue == Capture::CoordinateSelection)
		{
			captureValue = Capture::None;
		}
	}

	void ClickerController::finishCoordinateCapture(const ScreenPoint point)
	{
		if (captureValue != Capture::Coordinate)
		{
			return;
		}
		configValue.injector.lockedCoordinate = point;
		captureValue						  = Capture::None;
	}

	void ClickerController::handleKeyPressed(const std::uint32_t key)
	{
		if (captureValue == Capture::Coordinate || captureValue == Capture::CoordinateSelection)
		{
			if (key == VK_ESCAPE)
			{
				captureValue = Capture::None;
			}
			return;
		}
		if (captureValue == Capture::StartHotkey || captureValue == Capture::CustomKey)
		{
			const bool startHotkeyCapture = captureValue == Capture::StartHotkey;
			invalidCaptureKey =
				!keyMap.contains(key) || (startHotkeyCapture ? key == configValue.injector.customKey : key == configValue.startHotkey || isMouse(key));
			if (invalidCaptureKey)
			{
				return;
			}
			if (startHotkeyCapture)
			{
				configValue.startHotkey = key;
			}
			else
			{
				configValue.injector.customKey = key;
				if (isWheelKey(key))
				{
					configValue.injector.action = InputAction::Repeat;
				}
			}
			captureValue = Capture::None;
			return;
		}
		if (key == configValue.startHotkey)
		{
			toggleRunning();
		}
	}

	void ClickerController::toggleRunning()
	{
		if (running())
		{
			stop();
			return;
		}
		if (!canStart())
		{
			return;
		}
		// 调用后端前先标记运行状态，以便处理同步完成的情况。
		runStateValue = RunState::Running;
		try
		{
			if (!startOperation(configValue.injector))
			{
				runStateValue = RunState::Idle;
			}
		}
		catch (...)
		{
			runStateValue = RunState::Idle;
			throw;
		}
	}

	void ClickerController::stop()
	{
		if (runStateValue != RunState::Running)
		{
			return;
		}
		runStateValue = RunState::Stopping;
		try
		{
			stopOperation();
		}
		catch (...)
		{
			runStateValue = RunState::Running;
			throw;
		}
	}

	void ClickerController::executionFinished()
	{
		runStateValue = RunState::Idle;
	}
} // namespace flori_input
