/**
 * @file event_script_file.cpp
 * @brief 实现事件剧本文件的 Windows 平台操作。
 */
#include "platform/event_script_file.h"
#include <algorithm>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <shellapi.h>
#include <vector>
#include "core/event_script.h"

namespace flori_input::platform
{
	std::vector<std::string> listEventScripts(const std::filesystem::path &directory)
	{
		std::filesystem::create_directories(directory);
		std::vector<std::string> files;
		for (const auto &entry: std::filesystem::directory_iterator(directory))
		{
			const auto extension = entry.path().extension().wstring();
			if (!entry.is_regular_file() || (_wcsicmp(extension.c_str(), L".fsevent") && _wcsicmp(extension.c_str(), L".txt")))
			{
				continue;
			}
			const auto name = entry.path().filename().u8string();
			files.emplace_back(reinterpret_cast<const char *>(name.data()), name.size());
		}
		std::ranges::sort(files);
		return files;
	}
	std::optional<std::string> chooseEventScript(const HWND owner)
	{
		std::vector<wchar_t> file(32768);
		OPENFILENAMEW		 dialog{.lStructSize = sizeof(OPENFILENAMEW)};
		dialog.hwndOwner   = owner;
		dialog.lpstrFilter = L"Flori Input Event Script (*.fsevent)\0*.fsevent\0Text files (*.txt)\0*.txt\0All files\0*.*\0";
		dialog.lpstrFile   = file.data();
		dialog.nMaxFile	   = static_cast<DWORD>(file.size());
		dialog.Flags	   = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if (!GetOpenFileNameW(&dialog))
		{
			if (CommDlgExtendedError())
			{
				throw std::runtime_error("[[script.dialog_failed]]");
			}
			return {};
		}
		const auto utf8 = std::filesystem::path(file.data()).u8string();
		return std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size());
	}
	void editEventScript(const HWND owner, const std::string &path)
	{
		if (path.empty())
		{
			throw std::runtime_error("[[script.file_required]]");
		}
		const std::filesystem::path scriptPath(std::u8string(path.begin(), path.end()));
		const auto					argument = L"\"" + scriptPath.wstring() + L"\"";
		if (reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", L"notepad.exe", argument.c_str(), nullptr, SW_SHOWNORMAL)) <= 32)
		{
			throw std::runtime_error("[[script.editor_failed]]");
		}
	}
	Program loadEventScript(const std::string &path)
	{
		if (path.empty())
		{
			throw std::runtime_error("[[script.file_required]]");
		}
		const std::filesystem::path scriptPath(std::u8string(path.begin(), path.end()));
		std::ifstream				file(scriptPath, std::ios::binary | std::ios::ate);
		if (!file)
		{
			throw std::runtime_error("[[script.open_failed]]");
		}
		const auto size = file.tellg();
		if (size < 0 || size > 1024 * 1024)
		{
			throw std::runtime_error("[[script.file_size_limit]]");
		}
		std::string source(static_cast<std::size_t>(size), '\0');
		file.seekg(0);
		if (!file.read(source.data(), static_cast<std::streamsize>(source.size())))
		{
			throw std::runtime_error("[[script.read_failed]]");
		}
		return eventScriptProgram(source);
	}
} // namespace flori_input::platform
