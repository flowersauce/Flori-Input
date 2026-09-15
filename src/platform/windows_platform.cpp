/**
 * @file windows_platform.cpp
 * @brief 实现 Slint 主窗口所需的 Windows 原生能力。
 */
#include "platform/windows_platform.h"
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <slint.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <windowsx.h>
#include "platform/windows_input_marker.h"

namespace flori_input::platform
{
	namespace
	{
		constexpr UINT DispatchTaskMessage = WM_APP + 0x3A1;

		/** @brief 将 Raw Input 键盘记录转换为稳定的虚拟键码。 */
		std::uint32_t rawVirtualKey(const RAWKEYBOARD &keyboard)
		{
			if (keyboard.VKey == 0xFF)
			{
				return 0;
			}
			if (keyboard.VKey == VK_SHIFT)
			{
				return MapVirtualKeyW(keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);
			}
			if (keyboard.VKey == VK_CONTROL)
			{
				return keyboard.Flags & RI_KEY_E0 ? VK_RCONTROL : VK_LCONTROL;
			}
			return keyboard.VKey;
		}

		/** @brief 从当前模块查找指定 RCDATA 音频资源。 */
		const void *resource(const int id, DWORD &size)
		{
			const auto module = GetModuleHandleW(nullptr);
			const auto found  = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
			if (!found)
			{
				return nullptr;
			}
			size = SizeofResource(module, found);
			return LockResource(LoadResource(module, found));
		}
	} // namespace
	Assets::Assets() = default;
	Assets::~Assets()
	{
		PlaySoundW(nullptr, nullptr, 0);
	}
	void Assets::play(const bool start)
	{
		DWORD	   size{};
		const auto sound = resource(start ? 201 : 202, size);
		if (sound)
		{
			PlaySoundW(static_cast<LPCWSTR>(sound), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
		}
	}
	InstanceLock::InstanceLock(const std::filesystem::path &configFile)
	{
		std::error_code error;
		std::filesystem::create_directories(configFile.parent_path(), error);
		if (error)
		{
			errorCode = ERROR_ACCESS_DENIED;
			return;
		}
		// 保留旧锁名，避免新旧版本同时启动。
		const auto path = configFile.parent_path() / "FSClicker.lock";
		handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
		{
			errorCode = GetLastError();
		}
	}
	InstanceLock::~InstanceLock()
	{
		if (handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(handle);
		}
	}
	WindowIntegration::WindowIntegration(const HWND							   windowHandle,
										 std::function<void()>				   preferencesCallback,
										 std::function<void(std::uint32_t)>	   keyCallback,
										 std::function<void(std::string_view)> traceCallback,
										 std::function<void(bool)>			   activationCallback)
		: mainWindow(windowHandle)
		, preferencesChanged(std::move(preferencesCallback))
		, keyPressed(std::move(keyCallback))
		, inputTrace(std::move(traceCallback))
		, activationChanged(std::move(activationCallback))
	{
		if (!mainWindow || !IsWindow(mainWindow))
		{
			throw std::runtime_error("Cannot attach Windows preferences observer: invalid HWND");
		}
		if (GetWindowThreadProcessId(mainWindow, nullptr) != GetCurrentThreadId())
		{
			throw std::runtime_error("Cannot attach Windows preferences observer: window belongs to another thread");
		}
		if (!SetWindowSubclass(mainWindow, procedure, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<DWORD_PTR>(this)))
		{
			throw std::runtime_error("Cannot attach Windows preferences observer: SetWindowSubclass failed for a valid HWND on the current thread");
		}
		const RAWINPUTDEVICE keyboard{.usUsagePage = 0x01, .usUsage = 0x06, .dwFlags = RIDEV_INPUTSINK, .hwndTarget = mainWindow};
		if (!RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard)))
		{
			RemoveWindowSubclass(mainWindow, procedure, reinterpret_cast<UINT_PTR>(this));
			throw std::runtime_error("Cannot register global raw keyboard input");
		}
		rawKeyboardRegistered = true;
		const bool active	  = GetForegroundWindow() == mainWindow;
		if (activationChanged)
		{
			activationChanged(active);
		}
	}
	WindowIntegration::~WindowIntegration()
	{
		*alive = false;
		if (rawKeyboardRegistered)
		{
			constexpr RAWINPUTDEVICE keyboard{.usUsagePage = 0x01, .usUsage = 0x06, .dwFlags = RIDEV_REMOVE, .hwndTarget = nullptr};
			RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard));
			rawKeyboardRegistered = false;
		}
		MSG message{};
		while (PeekMessageW(&message, mainWindow, DispatchTaskMessage, DispatchTaskMessage, PM_REMOVE))
		{
			delete reinterpret_cast<std::function<void()> *>(message.lParam);
		}
		RemoveWindowSubclass(mainWindow, procedure, reinterpret_cast<UINT_PTR>(this));
	}
	LRESULT CALLBACK
	WindowIntegration::procedure(const HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam, UINT_PTR, const DWORD_PTR data)
	{
		auto *self = reinterpret_cast<WindowIntegration *>(data);
		if (message == WM_ACTIVATE)
		{
			const bool active	= LOWORD(wParam) != WA_INACTIVE;
			const auto weak		= std::weak_ptr(self->alive);
			const auto callback = self->activationChanged;
			slint::invoke_from_event_loop(
				[weak, callback, active]
				{
					if (const auto alive = weak.lock(); alive && *alive && callback)
					{
						callback(active);
					}
				});
		}
		if (message == DispatchTaskMessage)
		{
			if (self->inputTrace)
			{
				self->inputTrace("Win32 owner task received");
			}
			const std::unique_ptr<std::function<void()>> task(reinterpret_cast<std::function<void()> *>(lParam));
			if (task && *task)
			{
				// 原生消息分发可能嵌套在 Slint 布局或输入处理中。
				// 返回 Slint 事件循环后再更新界面。
				const auto weak = std::weak_ptr<bool>(self->alive);
				slint::invoke_from_event_loop(
					[weak, callback = std::move(*task)]
					{
						if (const auto alive = weak.lock(); alive && *alive)
						{
							try
							{
								callback();
							}
							catch (...)
							{
							}
						}
					});
			}
			return 0;
		}
		if (self->inputTrace)
		{
			if (message == WM_LBUTTONDOWN)
			{
				self->inputTrace("Win32 left button down");
			}
			if (message == WM_LBUTTONUP)
			{
				self->inputTrace("Win32 left button up");
			}
			if (message == WM_MOUSEMOVE && !self->tracedMouseMove)
			{
				self->tracedMouseMove = true;
				self->inputTrace("Win32 mouse move received");
			}
		}
		if (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED || message == WM_DISPLAYCHANGE)
		{
			const auto weak		= std::weak_ptr<bool>(self->alive);
			const auto callback = self->preferencesChanged;
			slint::invoke_from_event_loop(
				[weak, callback]
				{
					if (const auto alive = weak.lock(); alive && *alive)
					{
						callback();
					}
				});
		}
		if (message == WM_INPUT)
		{
			RAWINPUT input{};
			UINT	 size = sizeof(input);
			if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1)
				&& input.header.dwType == RIM_TYPEKEYBOARD && input.data.keyboard.ExtraInformation != InjectedInputMarker)
			{
				const auto key		 = rawVirtualKey(input.data.keyboard);
				const bool down		 = !(input.data.keyboard.Flags & RI_KEY_BREAK);
				const bool firstDown = key && key < self->keysDown.size() && down && !self->keysDown[key];
				if (key && key < self->keysDown.size())
				{
					self->keysDown[key] = down;
				}
				if (firstDown)
				{
					if (self->inputTrace)
					{
						self->inputTrace("Raw keyboard key-down=" + std::to_string(key));
					}
					if (self->keyPressed)
					{
						self->keyPressed(key);
					}
				}
			}
		}
		if (message == WM_NCHITTEST)
		{
			POINT point{.x = GET_X_LPARAM(lParam), .y = GET_Y_LPARAM(lParam)};
			RECT  client{};
			if (ScreenToClient(window, &point) && GetClientRect(window, &client))
			{
				const int  width  = client.right - client.left;
				const int  height = client.bottom - client.top;
				const RECT dragArea{
					.left = MulDiv(width, 12, 500), .top = MulDiv(height, 12, 360), .right = MulDiv(width, 392, 500), .bottom = MulDiv(height, 44, 360)};
				if (PtInRect(&dragArea, point))
				{
					return HTCAPTION;
				}
			}
		}
		return DefSubclassProc(window, message, wParam, lParam);
	}

	void WindowIntegration::dispatchTask(const HWND window, std::function<void()> task)
	{
		if (window && IsWindow(window))
		{
			auto owned = std::make_unique<std::function<void()>>(std::move(task));
			if (PostMessageW(window, DispatchTaskMessage, 0, reinterpret_cast<LPARAM>(owned.get())))
			{
				owned.release();
				return;
			}
			task = std::move(*owned);
		}
		slint::invoke_from_event_loop(std::move(task));
	}
	void WindowIntegration::openLink(const std::wstring_view link)
	{
		const std::wstring value(link);
		ShellExecuteW(nullptr, L"open", value.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}
	void WindowIntegration::showError(const HWND window, const std::wstring_view message)
	{
		const std::wstring value(message);
		MessageBoxW(window, value.c_str(), L"Flori Input", MB_OK | MB_ICONERROR);
	}
} // namespace flori_input::platform
