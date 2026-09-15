/**
 * @file windows_input_marker.h
 * @brief 定义用于识别本程序注入事件的 Windows 标记。
 */
#ifndef FLORI_INPUT_PLATFORM_WINDOWS_INPUT_MARKER_H
#define FLORI_INPUT_PLATFORM_WINDOWS_INPUT_MARKER_H

#include <windows.h>

namespace flori_input::platform
{
	inline constexpr ULONG_PTR InjectedInputMarker = 0x46535050;
}

#endif // FLORI_INPUT_PLATFORM_WINDOWS_INPUT_MARKER_H
