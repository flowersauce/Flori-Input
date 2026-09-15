/**
 * @file keyboard_hook.cpp
 * @brief 实现全局鼠标低级钩子和按键分派。
 */
#include "platform/keyboard_hook.h"
#include "platform/windows_input_marker.h"

#include <utility>

#include "core/input_types.h"

namespace flori_input
{
	std::atomic<KeyboardHook *> KeyboardHook::activeHook = nullptr;

	KeyboardHook::KeyboardHook(KeyCallback keyCallback, CoordinateCallback coordinateCallback, CoordinateMoveCallback moveCallback)
		: keyCallback(std::move(keyCallback))
		, coordinateCallback(std::move(coordinateCallback))
		, coordinateMoveCallback(std::move(moveCallback))
	{
	}

	KeyboardHook::~KeyboardHook()
	{
		uninstall();
	}

	bool KeyboardHook::install()
	{
		if (isInstalled())
		{
			return true;
		}

		KeyboardHook *expected = nullptr;
		if (!activeHook.compare_exchange_strong(expected, this, std::memory_order_acq_rel))
		{
			return false;
		}

		mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHookCallback, nullptr, 0);
		if (!isInstalled())
		{
			uninstall();
			return false;
		}
		return true;
	}

	void KeyboardHook::uninstall()
	{
		coordinateCaptureActive = false;
		if (mouseHook)
		{
			UnhookWindowsHookEx(mouseHook);
			mouseHook = nullptr;
		}
		releaseOwnership();
	}

	bool KeyboardHook::isInstalled() const
	{
		return mouseHook != nullptr;
	}
	void KeyboardHook::setCoordinateCaptureActive(const bool active)
	{
		coordinateCaptureActive = active;
	}

	void KeyboardHook::releaseOwnership()
	{
		auto expected = this;
		activeHook.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
	}

	LRESULT CALLBACK KeyboardHook::mouseHookCallback(const int code, const WPARAM message, const LPARAM eventData)
	{
		KeyboardHook *const hook = activeHook.load(std::memory_order_acquire);
		if (code >= 0 && hook)
		{
			const auto *mouseEvent = reinterpret_cast<const MSLLHOOKSTRUCT *>(eventData);
			if (mouseEvent->dwExtraInfo == platform::InjectedInputMarker)
			{
				return CallNextHookEx(nullptr, code, message, eventData);
			}
			switch (message)
			{
				case WM_MOUSEMOVE:
					if (hook->coordinateCaptureActive)
					{
						hook->dispatchCoordinateMove({mouseEvent->pt.x, mouseEvent->pt.y});
					}
					break;
				case WM_LBUTTONDOWN:
					if (hook->coordinateCaptureActive && hook->coordinateCallback)
					{
						hook->suppressLeftUp = true;
						hook->dispatchCoordinate({mouseEvent->pt.x, mouseEvent->pt.y}, true);
						return 1;
					}
					hook->dispatch(VK_LBUTTON);
					break;
				case WM_LBUTTONUP:
					if (hook->suppressLeftUp.exchange(false))
					{
						return 1;
					}
					break;
				case WM_MBUTTONDOWN:
					hook->dispatch(VK_MBUTTON);
					break;
				case WM_RBUTTONDOWN:
					if (hook->coordinateCaptureActive && hook->coordinateCallback)
					{
						hook->suppressRightUp = true;
						hook->dispatchCoordinate({mouseEvent->pt.x, mouseEvent->pt.y}, false);
						return 1;
					}
					hook->dispatch(VK_RBUTTON);
					break;
				case WM_RBUTTONUP:
					if (hook->suppressRightUp.exchange(false))
					{
						return 1;
					}
					break;
				case WM_MOUSEWHEEL:
					hook->dispatch(GET_WHEEL_DELTA_WPARAM(mouseEvent->mouseData) > 0 ? WheelUpKey : WheelDownKey);
					break;
				default:
					break;
			}
		}

		return CallNextHookEx(nullptr, code, message, eventData);
	}

	void KeyboardHook::dispatch(const std::uint32_t keyCode) const noexcept
	{
		if (!keyCallback)
		{
			return;
		}

		try
		{
			keyCallback(keyCode);
		}
		catch (...)
		{
		}
	}
	void KeyboardHook::dispatchCoordinate(const ScreenPoint point, const bool picked) const noexcept
	{
		try
		{
			coordinateCallback(point, picked);
		}
		catch (...)
		{
		}
	}
	void KeyboardHook::dispatchCoordinateMove(const ScreenPoint point) const noexcept
	{
		if (!coordinateMoveCallback)
		{
			return;
		}
		try
		{
			coordinateMoveCallback(point);
		}
		catch (...)
		{
		}
	}
} // namespace flori_input
