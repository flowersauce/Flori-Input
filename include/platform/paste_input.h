/**
 * @file paste_input.h
 * @brief 声明剪贴板快照和面向目标窗口的输入后端工厂。
 */
#ifndef FLORI_INPUT_PLATFORM_PASTE_INPUT_H
#define FLORI_INPUT_PLATFORM_PASTE_INPUT_H

#include <string>
#include <string_view>

#include "core/program_executor.h"
#include "platform/windows_input_backend.h"

namespace flori_input::platform
{
	/** @brief 读取当前剪贴板的 Unicode 文本快照。 */
	std::u16string clipboardTextSnapshot();
	/** @brief 将 Unicode 文本写入剪贴板，成功后由系统接管数据所有权。 */
	void writeClipboardText(HWND owner, std::wstring_view text);
	// 在工作线程启动前捕获目标窗口及键盘布局；等待触发键释放，并在每次输入前检查焦点与物理修饰键。
	/** @brief 创建限定目标窗口与键盘状态的输入后端工厂。 */
	InputBackendFactory pasteBackendFactory(HWND target, std::uint32_t trigger, WindowsInputBackend::InputSender sender = {});
} // namespace flori_input::platform

#endif // FLORI_INPUT_PLATFORM_PASTE_INPUT_H
