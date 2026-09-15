/**
 * @file windows_input_backend.cpp
 * @brief 实现键盘、鼠标事件转换和可靠释放。
 */
#include "platform/windows_input_backend.h"
#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <utility>
#include "platform/windows_input_marker.h"

namespace flori_input
{
	namespace
	{
		struct VirtualDesktop
		{
			int left;
			int top;
			int width;
			int height;
		};
		/** @brief 读取当前虚拟桌面边界，供校验与绝对坐标映射共用。 */
		VirtualDesktop desktopMetrics()
		{
			return {.left	= GetSystemMetrics(SM_XVIRTUALSCREEN),
					.top	= GetSystemMetrics(SM_YVIRTUALSCREEN),
					.width	= GetSystemMetrics(SM_CXVIRTUALSCREEN),
					.height = GetSystemMetrics(SM_CYVIRTUALSCREEN)};
		}
		/** @brief 同时排除虚拟桌面外部与多显示器之间的空白区域。 */
		bool visibleScreenPoint(const int x, const int y, const VirtualDesktop &desktop)
		{
			const auto inside = [](const int value, const int origin, const int size)
			{ return size > 0 && static_cast<std::int64_t>(value) - origin >= 0 && static_cast<std::int64_t>(value) - origin < size; };
			return inside(x, desktop.left, desktop.width) && inside(y, desktop.top, desktop.height)
				&& MonitorFromPoint(POINT{.x = x, .y = y}, MONITOR_DEFAULTTONULL) != nullptr;
		}
		/** @brief 为文件预检与执行时检查生成一致的简短坐标错误。 */
		std::string moveError(const int x, const int y)
		{
			return "move(" + std::to_string(x) + ", " + std::to_string(y) + "[[input.outside_displays]]";
		}
		/** @brief 将平台无关输入事件转换为 Win32 INPUT。 */
		INPUT nativeEvent(const KeyboardEvent &event)
		{
			INPUT native{};
			using Kind = KeyboardEvent::Kind;
			if (event.kind == Kind::MouseButton || event.kind == Kind::MouseMove || event.kind == Kind::Wheel)
			{
				native.type			  = INPUT_MOUSE;
				native.mi.dwExtraInfo = platform::InjectedInputMarker;
				if (event.kind == Kind::MouseButton)
				{
					if (event.code > static_cast<std::uint16_t>(MouseButton::Right))
					{
						throw InputError("[[input.invalid_button]]");
					}
					constexpr DWORD down[]{MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_RIGHTDOWN};
					constexpr DWORD up[]{MOUSEEVENTF_LEFTUP, MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_RIGHTUP};
					native.mi.dwFlags = event.up ? up[event.code] : down[event.code];
				}
				else if (event.kind == Kind::Wheel)
				{
					if (event.y == 0 || event.y < -MaximumWheelSteps || event.y > MaximumWheelSteps)
					{
						throw InputError("[[input.invalid_wheel]]");
					}
					native.mi.dwFlags	= MOUSEEVENTF_WHEEL;
					native.mi.mouseData = static_cast<DWORD>(event.y * WHEEL_DELTA);
				}
				else
				{
					const auto desktop = desktopMetrics();
					if (!visibleScreenPoint(event.x, event.y, desktop))
					{
						throw InputError(moveError(event.x, event.y));
					}
					const auto coordinate = [](const int value, const int origin, const int size)
					{ return size <= 1 ? 0L : static_cast<LONG>(std::clamp((static_cast<long long>(value) - origin) * 65535 / (size - 1), 0LL, 65535LL)); };
					native.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
					native.mi.dx	  = coordinate(event.x, desktop.left, desktop.width);
					native.mi.dy	  = coordinate(event.y, desktop.top, desktop.height);
				}
				return native;
			}
			native.type		= INPUT_KEYBOARD;
			native.ki.wScan = event.code;
			native.ki.dwFlags =
				(event.unicode ? KEYEVENTF_UNICODE : KEYEVENTF_SCANCODE) | (event.extended ? KEYEVENTF_EXTENDEDKEY : 0) | (event.up ? KEYEVENTF_KEYUP : 0);
			native.ki.dwExtraInfo = platform::InjectedInputMarker;
			if (event.kind == Kind::VirtualKey)
			{
				native.ki.wVk	  = event.code;
				native.ki.wScan	  = 0;
				native.ki.dwFlags = event.up ? KEYEVENTF_KEYUP : 0;
			}
			return native;
		}
		/** @brief 根据按下序列构造配对的释放序列。 */
		Stroke complete(std::vector<KeyboardEvent> down, const bool reverse)
		{
			auto up = down;
			if (reverse)
			{
				std::ranges::reverse(up);
			}
			for (auto &event: up)
			{
				event.up = true;
			}
			return {.down = std::move(down), .up = std::move(up)};
		}
	} // namespace
	namespace platform
	{
		void validateVisibleMouseMoves(const Program &program)
		{
			const auto desktop = desktopMetrics();
			for (const auto &node: program.nodes())
			{
				const auto *move = std::get_if<MouseMove>(&node);
				if (move && !visibleScreenPoint(move->x, move->y, desktop))
				{
					const auto position = move->line ? std::to_string(move->line) + ":" + std::to_string(move->column) + " " : std::string{};
					throw InputError(position + moveError(move->x, move->y));
				}
			}
		}
	} // namespace platform
	WindowsInputBackend::WindowsInputBackend(HKL layout, InputSender sender)
		: layout_(layout)
		, sender_(std::move(sender))
	{
		if (!sender_)
		{
			sender_ = [](std::span<const INPUT> events)
			{
				SetLastError(ERROR_SUCCESS);
				const UINT accepted = SendInput(static_cast<UINT>(events.size()), const_cast<INPUT *>(events.data()), sizeof(INPUT));
				return SendResult{.accepted = accepted, .error = GetLastError()};
			};
		}
	}
	WindowsInputBackend::~WindowsInputBackend()
	{
		releaseAll();
	}

	KeyboardEvent WindowsInputBackend::physical(const std::uint16_t virtualKey) const
	{
		const UINT scan = MapVirtualKeyExW(virtualKey, MAPVK_VK_TO_VSC_EX, layout_);
		if (!scan || (scan & 0xff00) == 0xe100)
		{
			throw InputError("[[input.unsupported_scan_code]]");
		}
		return {.code = static_cast<std::uint16_t>(scan & 0xff), .unicode = false, .extended = (scan & 0xff00) == 0xe000, .up = false};
	}
	Stroke WindowsInputBackend::key(const std::uint16_t virtualKey)
	{
		return complete({physical(virtualKey)}, true);
	}
	Stroke WindowsInputBackend::character(char32_t value, const TextInputStrategy strategy)
	{
		if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
		{
			throw InputError("[[input.invalid_unicode]]");
		}
		if (value == U'\n' || value == U'\r' || value == U'\t')
		{
			return key(value == U'\t' ? VK_TAB : VK_RETURN);
		}
		if (strategy == TextInputStrategy::PreferPhysical && value <= 0xffff && !(GetKeyState(VK_CAPITAL) & 1))
		{
			const SHORT mapping = VkKeyScanExW(static_cast<wchar_t>(value), layout_);
			if (mapping != -1 && ((mapping >> 8) & ~1) == 0)
			{
				const WORD vk = mapping & 0xff;
				BYTE	   state[256]{};
				if (mapping & 0x100)
				{
					state[VK_SHIFT] = 0x80;
				}
				wchar_t	   output[8]{};
				const UINT scan = MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC, layout_);
				// 标志位 4 避免改变调用线程的死键状态。
				if (scan && ToUnicodeEx(vk, scan, state, output, 8, 4, layout_) == 1 && output[0] == value)
				{
					std::vector<KeyboardEvent> down;
					if (mapping & 0x100)
					{
						down.push_back(physical(VK_LSHIFT));
					}
					down.push_back(physical(vk));
					return complete(std::move(down), true);
				}
			}
		}
		if (value <= 0xffff)
		{
			return complete({KeyboardEvent{.code = static_cast<std::uint16_t>(value), .unicode = true}}, false);
		}
		value -= 0x10000;
		return complete({KeyboardEvent{.code = static_cast<std::uint16_t>(0xd800 + (value >> 10)), .unicode = true},
						 KeyboardEvent{.code = static_cast<std::uint16_t>(0xdc00 + (value & 0x3ff)), .unicode = true}},
						false);
	}
	void WindowsInputBackend::send(std::span<const KeyboardEvent> events)
	{
		if (events.empty())
		{
			return;
		}
		if (events.size() > std::numeric_limits<UINT>::max())
		{
			throw InputError("[[input.batch_limit]]");
		}
		constexpr std::size_t				   InlineInputCapacity = 16;
		std::array<INPUT, InlineInputCapacity> inlineNative;
		std::vector<INPUT>					   allocatedNative;
		std::span<INPUT>					   native;
		if (events.size() <= inlineNative.size())
		{
			native = {inlineNative.data(), events.size()};
		}
		else
		{
			allocatedNative.resize(events.size());
			native = allocatedNative;
		}
		for (std::size_t i = 0; i < events.size(); ++i)
		{
			native[i] = nativeEvent(events[i]);
		}
		submit(events, native);
	}
	void WindowsInputBackend::submit(std::span<const KeyboardEvent> events, std::span<const INPUT> native)
	{
		// 所有内存分配均在注入前完成，记录已接受的按下事件时不会再分配内存。
		held_.reserve(held_.size() + events.size());
		const auto [accepted, error] = sender_(native);
		if (accepted > events.size())
		{
			throw InputError("[[input.invalid_sender_count]]");
		}
		accepted_ += accepted;
		for (std::size_t i = 0; i < accepted; ++i)
		{
			auto event = events[i];
			if (event.kind == KeyboardEvent::Kind::MouseMove || event.kind == KeyboardEvent::Kind::Wheel)
			{
				continue;
			}
			if (!event.up)
			{
				held_.push_back(event);
			}
			else
			{
				event.up		 = false;
				const auto found = std::ranges::find(held_, event);
				if (found != held_.end())
				{
					held_.erase(found);
				}
			}
		}
		if (accepted != events.size())
		{
			throw InputError("[[input.send_failed]]" + std::to_string(error) + "[[input.uipi_hint]]");
		}
	}
	bool WindowsInputBackend::releaseAll() noexcept
	{
		bool success = true;
		// 保留释放失败的事件，供后续显式清理或析构时重试。
		for (std::size_t i = held_.size(); i > 0; --i)
		{
			auto event	 = held_[i - 1];
			event.up	 = true;
			INPUT native = nativeEvent(event);
			try
			{
				const auto [accepted, error] = sender_(std::span<const INPUT>(&native, 1));
				if (accepted == 1)
				{
					++cleanup_;
					held_.erase(held_.begin() + static_cast<std::ptrdiff_t>(i - 1));
				}
				else
				{
					cleanupError_ = error;
					success		  = false;
				}
			}
			catch (...)
			{
				cleanupError_ = ERROR_UNHANDLED_EXCEPTION;
				success		  = false;
			}
		}
		return success;
	}
} // namespace flori_input
