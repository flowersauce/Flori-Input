/**
 * @file app_config.cpp
 * @brief 实现 JSONC 配置读写、归一化和系统偏好读取。
 */
#include "app/app_config.h"
#include <algorithm>
#include <atomic>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#include "app/clicker_runtime.h"
#include "core/json.h"

namespace flori_input
{
	namespace
	{
		using Json	 = json::Value;
		namespace fs = std::filesystem;

		/** @brief 将最近一次 Windows 错误转换为标准错误码。 */
		std::error_code windowsError()
		{
			return {static_cast<int>(GetLastError()), std::system_category()};
		}

		struct File
		{
			HANDLE handle{INVALID_HANDLE_VALUE};
			~File()
			{
				close();
			}
			/** @brief 关闭当前拥有的文件句柄。 */
			void close()
			{
				if (handle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(std::exchange(handle, INVALID_HANDLE_VALUE));
				}
			}
		};

		/** @brief 完整读取配置文件并返回 I/O 错误。 */
		bool readFile(const fs::path &path, std::string &text, std::error_code &error)
		{
			const File file{
				CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
			if (file.handle == INVALID_HANDLE_VALUE)
			{
				error = windowsError();
				return false;
			}
			char  buffer[8192];
			DWORD count{};
			for (;;)
			{
				if (!ReadFile(file.handle, buffer, sizeof(buffer), &count, nullptr))
				{
					error = windowsError();
					return false;
				}
				if (count == 0)
				{
					return true;
				}
				text.append(buffer, count);
			}
		}

		/** @brief 写入同目录临时文件并原子替换原配置。 */
		bool writeAtomically(const fs::path &path, std::string_view text, std::error_code &error)
		{
			fs::create_directories(path.parent_path(), error);
			if (error)
			{
				return false;
			}
			static std::atomic<unsigned long long> sequence{};
			fs::path							   temporary;
			File								   file;
			for (int attempt = 0; attempt < 100; ++attempt)
			{
				temporary = path;
				temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++sequence);
				file.handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (file.handle != INVALID_HANDLE_VALUE)
				{
					break;
				}
				if (const auto code = GetLastError(); code != ERROR_FILE_EXISTS && code != ERROR_ALREADY_EXISTS)
				{
					error = {static_cast<int>(code), std::system_category()};
					return false;
				}
			}
			if (file.handle == INVALID_HANDLE_VALUE)
			{
				error = std::make_error_code(std::errc::file_exists);
				return false;
			}
			// 即使重置失败，也不得删除或截断目标文件。
			struct Cleanup
			{
				File		   &file;
				const fs::path &path;
				~Cleanup()
				{
					file.close();
					DeleteFileW(path.c_str());
				}
			};
			const Cleanup cleanup{.file = file, .path = temporary};
			while (!text.empty())
			{
				DWORD written{};
				if (const auto count = static_cast<DWORD>(std::min<std::size_t>(text.size(), MAXDWORD));
					!WriteFile(file.handle, text.data(), count, &written, nullptr))
				{
					error = windowsError();
					return false;
				}
				if (written == 0)
				{
					error = std::make_error_code(std::errc::io_error);
					return false;
				}
				text.remove_prefix(written);
			}
			if (!FlushFileBuffers(file.handle))
			{
				error = windowsError();
				return false;
			}
			file.close();
			if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				error = windowsError();
				return false;
			}
			return true;
		}

		/** @brief 读取可精确表示为 int 的 JSON 数字字段。 */
		std::optional<int> integer(const Json &object, const char *name)
		{
			const auto it = json::find(object, name);
			if (!it || !it->is_number())
			{
				return {};
			}
			const double value = it->get_number();
			if (!std::isfinite(value) || std::trunc(value) != value || value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
			{
				return {};
			}
			return static_cast<int>(value);
		}

		/** @brief 读取有限浮点数字段，失败时返回默认值。 */
		double number(const Json &object, const char *name, const double fallback)
		{
			const auto it = json::find(object, name);
			if (!it || !it->is_number())
			{
				return fallback;
			}
			const double value = it->get_number();
			return std::isfinite(value) ? value : fallback;
		}

		/** @brief 将界面选项约束到支持的档位。 */
		AppSettings normalizedSettings(AppSettings settings)
		{
			settings.languageMode = std::clamp(settings.languageMode, 0, 2);
			settings.uiScaleIndex = std::clamp(settings.uiScaleIndex, 0, 2);
			settings.themeMode	  = std::clamp(settings.themeMode, 0, 2);
			return settings;
		}

		/** @brief 修正模拟粘贴热键和时序范围。 */
		PasteSettings normalizedPaste(PasteSettings settings)
		{
			if (settings.hotkey < VK_BACK || settings.hotkey > 0xfe || settings.hotkey == VK_PACKET)
			{
				settings.hotkey = DefaultStartHotkey;
			}
			settings.pressSeconds	 = std::isfinite(settings.pressSeconds) ? std::clamp(settings.pressSeconds, 0.0, 60.0) : 0.005;
			settings.intervalSeconds = std::isfinite(settings.intervalSeconds) ? std::clamp(settings.intervalSeconds, 0.0, 60.0) : 0.01;
			return settings;
		}

		/** @brief 将 JSONC 文本解析为各功能配置。 */
		bool decode(const std::string_view text, AppSettings &settings, ClickerController::Config &config, PasteSettings &paste, EventScriptSettings &script)
		{
			Json root;
			try
			{
				root = json::parse<true>(text);
			}
			catch (const std::runtime_error &)
			{
				return false;
			}
			if (!root.is_object())
			{
				return false;
			}
			const auto settingsObject = json::find(root, "settings");
			const auto functionObject = json::find(root, "function");
			if (!settingsObject || !settingsObject->is_object() || !functionObject || !functionObject->is_object())
			{
				return false;
			}
			const auto mode	 = integer(*settingsObject, "languageMode");
			const auto scale = integer(*settingsObject, "uiScaleIndex");
			const auto theme = integer(*settingsObject, "themeMode");
			if (!mode || !scale || !theme)
			{
				return false;
			}
			settings = normalizedSettings({.languageMode = *mode, .uiScaleIndex = *scale, .themeMode = *theme});
			// 读取旧键以兼容既有配置；保存时仅写入新键。
			config.startHotkey = static_cast<std::uint32_t>(
				integer(*functionObject, "startHotkey").value_or(integer(*functionObject, "globalSwitchKey").value_or(DefaultStartHotkey)));
			auto &[inputKey, action, cursorMode, customKey, lockedCoordinate, cycle, pressDuration, timingJitterPercent] = config.injector;
			customKey				  = static_cast<std::uint32_t>(integer(*functionObject, "diyKey").value_or(0));
			inputKey				  = static_cast<InputKey>(std::clamp(integer(*functionObject, "inputKey").value_or(0), 0, 3));
			action					  = static_cast<InputAction>(std::clamp(integer(*functionObject, "inputActionMode").value_or(0), 0, 1));
			cursorMode				  = static_cast<CursorMode>(std::clamp(integer(*functionObject, "cursorMoveMode").value_or(0), 0, 1));
			lockedCoordinate		  = {.x = integer(*functionObject, "lockedX").value_or(0), .y = integer(*functionObject, "lockedY").value_or(0)};
			const double seconds	  = std::max(number(*functionObject, "cycleSeconds", 0.01), 0.001);
			cycle					  = seconds >= std::chrono::duration<double>(MaximumInputCycle).count()
									? MaximumInputCycle
									: std::chrono::nanoseconds(static_cast<std::int64_t>(std::round(seconds * 1e9)));
			const double pressSeconds = std::max(number(*functionObject, "pressDurationSeconds", 0.005), 0.0);
			pressDuration			  = pressSeconds >= std::chrono::duration<double>(MaximumPressDuration).count()
							? MaximumPressDuration
							: std::chrono::nanoseconds(static_cast<std::int64_t>(std::round(pressSeconds * 1e9)));
			timingJitterPercent		  = number(*functionObject, "timingJitterPercent", 0.0);
			config					  = ClickerController::normalizedConfig(config);
			if (const auto pasteObject = json::find(root, "pseudoPaste"); pasteObject && pasteObject->is_object())
			{
				paste.hotkey		  = static_cast<std::uint32_t>(integer(*pasteObject, "hotkey").value_or(DefaultStartHotkey));
				paste.pressSeconds	  = number(*pasteObject, "pressSeconds", 0.005);
				paste.intervalSeconds = number(*pasteObject, "intervalSeconds", 0.01);
				if (const auto unicodeValue = json::find(*pasteObject, "unicode"); unicodeValue && unicodeValue->is_boolean())
				{
					paste.unicode = unicodeValue->get_boolean();
				}
				paste = normalizedPaste(paste);
			}
			if (const auto scriptObject = json::find(root, "eventScript"); scriptObject && scriptObject->is_object())
			{
				if (const auto fileValue = json::find(*scriptObject, "file"); fileValue && fileValue->is_string())
				{
					script.file = fileValue->get_string();
				}
				const auto key = integer(*scriptObject, "hotkey").value_or(DefaultStartHotkey);
				script.hotkey  = key >= VK_BACK && key <= 0xfe && key != VK_PACKET ? key : DefaultStartHotkey;
				if (const auto limits = json::find(*scriptObject, "limits"); limits && limits->is_object())
				{
					const auto finiteSteps = integer(*limits, "maxFiniteSteps");
					const auto noWaitSteps = integer(*limits, "maxConsecutiveNoWaitSteps");
					if (finiteSteps && *finiteSteps > 0)
					{
						script.limits.maxFiniteSteps = static_cast<std::uint32_t>(*finiteSteps);
					}
					if (noWaitSteps && *noWaitSteps > 0)
					{
						script.limits.maxConsecutiveNoWaitSteps = static_cast<std::uint32_t>(*noWaitSteps);
					}
					script.limits = normalizedScriptExecutionLimits(script.limits);
				}
			}
			return true;
		}

		std::string
		encode(const AppSettings &settings, const ClickerController::Config &config, const PasteSettings &paste = {}, const EventScriptSettings &script = {})
		{
			const auto &[inputKey, action, cursorMode, customKey, lockedCoordinate, cycle, pressDuration, timingJitterPercent] = config.injector;
			return std::string("// 事件剧本执行保护；仅影响事件剧本，不影响触发器与模拟粘贴。\n"
							   "// 以下字段无效或超出范围时分别恢复默认值。\n"
							   "// eventScript.limits.maxFiniteSteps：有限剧本总执行步数，范围 1–1000000，默认 100000。\n"
							   "// eventScript.limits.maxConsecutiveNoWaitSteps：连续无正时间等待步数，范围 1–10000，默认 1000。\n")
				+ json::write(Json{
					{"eventScript",
					 {{"file", script.file},
					  {"hotkey", script.hotkey},
					  {"limits", {{"maxFiniteSteps", script.limits.maxFiniteSteps}, {"maxConsecutiveNoWaitSteps", script.limits.maxConsecutiveNoWaitSteps}}}}},
					{"pseudoPaste",
					 {{"hotkey", paste.hotkey}, {"pressSeconds", paste.pressSeconds}, {"intervalSeconds", paste.intervalSeconds}, {"unicode", paste.unicode}}},
					{"settings", {{"languageMode", settings.languageMode}, {"uiScaleIndex", settings.uiScaleIndex}, {"themeMode", settings.themeMode}}},
					{"function",
					 {{"startHotkey", config.startHotkey},
					  {"diyKey", customKey},
					  {"inputKey", static_cast<int>(inputKey)},
					  {"inputActionMode", static_cast<int>(action)},
					  {"cursorMoveMode", static_cast<int>(cursorMode)},
					  {"lockedX", lockedCoordinate.x},
					  {"lockedY", lockedCoordinate.y},
					  {"cycleSeconds", std::chrono::duration<double>(cycle).count()},
					  {"pressDurationSeconds", std::chrono::duration<double>(pressDuration).count()},
					  {"timingJitterPercent", timingJitterPercent}}}})
				+ '\n';
		}
	} // namespace

	std::filesystem::path AppConfig::executableDirectory()
	{
		std::vector<wchar_t> buffer(512);
		for (;;)
		{
			const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (length == 0)
			{
				throw std::system_error(windowsError(), "GetModuleFileNameW");
			}
			if (length < buffer.size())
			{
				return fs::path(std::wstring(buffer.data(), length)).parent_path();
			}
			buffer.resize(buffer.size() * 2);
		}
	}

	std::filesystem::path AppConfig::configPathFor(const fs::path &directory)
	{
		auto app = fs::absolute(directory).lexically_normal();
		if (!app.has_filename())
		{
			app = app.parent_path();
		}
		const bool installed =
			_wcsicmp(app.filename().c_str(), L"current") == 0 && fs::exists(app / "sq.version") && fs::exists(app.parent_path() / "Update.exe");
		return (installed ? app.parent_path() : app) / "config" / "config.jsonc";
	}

	SystemPreferences AppConfig::windowsPreferences()
	{
		SystemPreferences result;
		result.chineseUi = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
		ULONG count{}, size{};
		if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &size) && size > 0)
		{
			std::vector<wchar_t> languages(size);
			if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, languages.data(), &size))
			{
				result.chineseUi = false;
				for (const wchar_t *language = languages.data(); *language; language += wcslen(language) + 1)
				{
					if (_wcsnicmp(language, L"zh", 2) == 0 && (language[2] == L'-' || language[2] == L'\0'))
					{
						result.chineseUi = true;
					}
				}
			}
		}
		DWORD light{}, bytes = sizeof(light);
		if (RegGetValueW(HKEY_CURRENT_USER,
						 L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
						 L"AppsUseLightTheme",
						 RRF_RT_REG_DWORD,
						 nullptr,
						 &light,
						 &bytes)
			== ERROR_SUCCESS)
		{
			result.darkTheme = light == 0;
		}
		return result;
	}

	AppConfig::AppConfig(const fs::path &directory, PreferencesReader reader)
		: configPath(configPathFor(directory))
		, preferencesReader(std::move(reader))
	{
		if (!preferencesReader)
		{
			throw std::invalid_argument("[[config.reader_required]]");
		}
		refreshSystemPreferences();
	}

	AppConfig::LoadResult AppConfig::load()
	{
		error.clear();
		const bool exists = fs::exists(configPath, error);
		if (error)
		{
			return LoadResult::IoError;
		}
		std::string text;
		if (!exists)
		{
			if (!writeAtomically(configPath, encode({}, {}), error))
			{
				return LoadResult::IoError;
			}
			settingsValue	 = {};
			functionValue	 = {};
			pasteValue		 = {};
			eventScriptValue = {};
			return LoadResult::Created;
		}
		else if (!readFile(configPath, text, error))
		{
			return LoadResult::IoError;
		}
		AppSettings				  settings;
		ClickerController::Config function;
		PasteSettings			  paste;
		EventScriptSettings		  script;
		const bool				  valid = decode(text, settings, function, paste, script);
		if (!valid)
		{
			if (!writeAtomically(configPath, encode({}, {}), error))
			{
				return LoadResult::IoError;
			}
			settings = {};
			function = {};
			paste	 = {};
			script	 = {};
		}
		settingsValue	 = settings;
		functionValue	 = function;
		pasteValue		 = paste;
		eventScriptValue = script;
		return valid ? LoadResult::Loaded : LoadResult::Reset;
	}

	bool AppConfig::save()
	{
		error.clear();
		return writeAtomically(configPath, encode(settingsValue, functionValue, pasteValue, eventScriptValue), error);
	}

	bool AppConfig::save(const ClickerRuntime &runtime)
	{
		setFunctionConfig(runtime.controller().config());
		return save();
	}

	bool AppConfig::applyTo(const ClickerRuntime &runtime) const
	{
		bool applied = false;
		runtime.update([&](ClickerController &controller) { applied = controller.setConfig(functionValue); });
		return applied;
	}

	void AppConfig::setSettings(const AppSettings settings)
	{
		settingsValue = normalizedSettings(settings);
	}
	void AppConfig::setPasteSettings(const PasteSettings &settings)
	{
		pasteValue = normalizedPaste(settings);
	}
	void AppConfig::setEventScriptSettings(EventScriptSettings settings)
	{
		if (settings.hotkey < VK_BACK || settings.hotkey > 0xfe || settings.hotkey == VK_PACKET)
		{
			settings.hotkey = DefaultStartHotkey;
		}
		settings.limits	 = normalizedScriptExecutionLimits(settings.limits);
		eventScriptValue = std::move(settings);
	}
	void AppConfig::setFunctionConfig(const ClickerController::Config &config)
	{
		functionValue = ClickerController::normalizedConfig(config);
	}
	std::string_view AppConfig::language() const
	{
		return settingsValue.languageMode == 1 || (settingsValue.languageMode == 0 && preferences.chineseUi) ? "zh" : "en";
	}
	double AppConfig::uiScale() const
	{
		return 1.0 + settingsValue.uiScaleIndex * 0.25;
	}
	bool AppConfig::darkTheme() const
	{
		return settingsValue.themeMode == 1 || (settingsValue.themeMode == 0 && preferences.darkTheme);
	}
	void AppConfig::refreshSystemPreferences()
	{
		preferences = preferencesReader();
	}
} // namespace flori_input
