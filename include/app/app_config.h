/**
 * @file app_config.h
 * @brief 定义应用配置模型及持久化服务。
 */
#ifndef FLORI_INPUT_APP_APP_CONFIG_H
#define FLORI_INPUT_APP_APP_CONFIG_H

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include "core/clicker_controller.h"
#include "core/input_types.h"
#include "core/script_execution_limits.h"

namespace flori_input
{
	class ClickerRuntime;

	struct AppSettings
	{
		int			languageMode{}; // 0 自动，1 中文，2 英文
		int			uiScaleIndex{}; // 0 为 100%，1 为 125%，2 为 150%
		int			themeMode{};	// 0 自动，1 深色，2 浅色
		friend bool operator==(const AppSettings &, const AppSettings &) = default;
	};

	struct SystemPreferences
	{
		bool chineseUi{};
		bool darkTheme{true};
	};

	struct PasteSettings
	{
		std::uint32_t hotkey{DefaultStartHotkey}; // 仅支持键盘热键。
		double		  pressSeconds{0.005};
		double		  intervalSeconds{0.01};
		bool		  unicode{false};			  // 默认优先物理按键，无法映射时由后端回退 Unicode。
		friend bool	  operator==(const PasteSettings &, const PasteSettings &) = default;
	};
	struct EventScriptSettings
	{
		std::string			  file;
		std::uint32_t		  hotkey{DefaultStartHotkey};
		ScriptExecutionLimits limits;
		friend bool			  operator==(const EventScriptSettings &, const EventScriptSettings &) = default;
	};

	// 所有者线程上的配置服务；构造时仅解析路径，不写入文件。
	// 加载与保存须显式调用，析构时不执行 I/O。
	class AppConfig final
	{
	public:
		enum class LoadResult
		{
			Loaded,
			Created,
			Reset,
			IoError
		};
		using PreferencesReader = std::function<SystemPreferences()>;

		/** @brief 获取当前可执行文件所在目录。 */
		static std::filesystem::path executableDirectory();
		/** @brief 根据便携或安装目录规则确定配置路径。 */
		static std::filesystem::path configPathFor(const std::filesystem::path &applicationDirectory);
		/** @brief 从 Windows 读取语言及主题偏好。 */
		static SystemPreferences windowsPreferences();
		/** @brief 初始化配置服务；构造时不写入磁盘。 */
		explicit AppConfig(const std::filesystem::path &applicationDirectory = executableDirectory(), PreferencesReader preferencesReader = windowsPreferences);

		/** @brief 读取 JSONC 配置，必要时创建或重置默认值。 */
		LoadResult load();
		/** @brief 原子写入当前配置。 */
		bool save();
		/** @brief 同步触发器运行配置后原子写入。 */
		bool save(const ClickerRuntime &runtime);
		/** @brief 将保存的触发器配置应用到运行时。 */
		bool applyTo(const ClickerRuntime &runtime) const;
		/** @brief 获取当前配置文件路径。 */
		const std::filesystem::path &filePath() const
		{
			return configPath;
		}
		/** @brief 获取最近一次文件操作错误码。 */
		std::error_code lastError() const
		{
			return error;
		}
		/** @brief 获取当前界面设置。 */
		const AppSettings &settings() const
		{
			return settingsValue;
		}
		/** @brief 获取保存的触发器配置。 */
		const ClickerController::Config &functionConfig() const
		{
			return functionValue;
		}
		/** @brief 归一化并更新界面设置。 */
		void setSettings(AppSettings settings);
		/** @brief 获取模拟粘贴设置。 */
		const PasteSettings &pasteSettings() const
		{
			return pasteValue;
		}
		/** @brief 归一化并更新模拟粘贴设置。 */
		void setPasteSettings(const PasteSettings &settings);
		/** @brief 获取事件剧本设置。 */
		const EventScriptSettings &eventScriptSettings() const
		{
			return eventScriptValue;
		}
		/** @brief 归一化并更新事件剧本设置。 */
		void setEventScriptSettings(EventScriptSettings settings);
		/** @brief 归一化并更新触发器配置。 */
		void setFunctionConfig(const ClickerController::Config &config);
		/** @brief 解析语言的自动或手动选择。 */
		std::string_view language() const;
		/** @brief 将界面缩放档位转换为倍率。 */
		double uiScale() const;
		/** @brief 解析主题的自动或手动选择。 */
		bool darkTheme() const;
		// 平台报告系统偏好变更时，在所有者线程调用。
		/** @brief 刷新系统语言和主题偏好缓存。 */
		void refreshSystemPreferences();

	private:
		std::filesystem::path	  configPath;
		PreferencesReader		  preferencesReader;
		SystemPreferences		  preferences;
		AppSettings				  settingsValue;
		PasteSettings			  pasteValue;
		EventScriptSettings		  eventScriptValue;
		ClickerController::Config functionValue;
		std::error_code			  error;
	};
} // namespace flori_input

#endif // FLORI_INPUT_APP_APP_CONFIG_H
