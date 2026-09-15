/**
 * @file event_script_file.h
 * @brief 声明事件剧本文件的枚举、选择、编辑和加载操作。
 */
#ifndef FLORI_INPUT_PLATFORM_EVENT_SCRIPT_FILE_H
#define FLORI_INPUT_PLATFORM_EVENT_SCRIPT_FILE_H
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>
#include "core/input_program.h"

namespace flori_input::platform
{
	/** @brief 通过系统对话框选择事件剧本文件。 */
	std::optional<std::string> chooseEventScript(const HWND owner);
	/** @brief 枚举目录第一层可用的事件剧本。 */
	std::vector<std::string> listEventScripts(const std::filesystem::path &directory);
	/** @brief 使用外部编辑器打开指定剧本。 */
	void editEventScript(const HWND owner, const std::string &path);
	/** @brief 读取并解析 UTF-8 事件剧本。 */
	Program loadEventScript(const std::string &path);
} // namespace flori_input::platform

#endif // FLORI_INPUT_PLATFORM_EVENT_SCRIPT_FILE_H
