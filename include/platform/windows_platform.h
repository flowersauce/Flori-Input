/**
 * @file windows_platform.h
 * @brief 定义窗口、资源、单实例和原生消息集成。
 */
#ifndef FLORI_INPUT_PLATFORM_WINDOWS_PLATFORM_H
#define FLORI_INPUT_PLATFORM_WINDOWS_PLATFORM_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <windows.h>

namespace flori_input::platform
{
	class Assets final
	{
	public:
		/** @brief 加载程序内置的提示音资源。 */
		Assets();
		/** @brief 释放音频资源。 */
		~Assets();
		/** @brief 播放触发器启动或停止提示音。 */
		static void play(bool start);
	};
	class InstanceLock final
	{
	public:
		/** @brief 依据配置文件路径获取单实例锁。 */
		explicit InstanceLock(const std::filesystem::path &configFile);
		/** @brief 释放单实例锁。 */
		~InstanceLock();
		/** @brief 判断当前实例是否取得锁。 */
		bool acquired() const
		{
			return handle != INVALID_HANDLE_VALUE;
		}
		/** @brief 获取锁创建失败时的 Windows 错误码。 */
		DWORD error() const
		{
			return errorCode;
		}

	private:
		HANDLE handle{INVALID_HANDLE_VALUE};
		DWORD  errorCode{};
	};
	class WindowIntegration final
	{
	public:
		/** @brief 挂接主窗口消息并注册全局原始键盘输入。 */
		WindowIntegration(HWND									windowHandle,
						  std::function<void()>					preferencesCallback,
						  std::function<void(std::uint32_t)>	keyCallback,
						  std::function<void(std::string_view)> traceCallback	   = {},
						  std::function<void(bool)>				activationCallback = {});
		/** @brief 撤销窗口子类化和原始输入注册。 */
		~WindowIntegration();
		WindowIntegration(const WindowIntegration &)			= delete;
		WindowIntegration &operator=(const WindowIntegration &) = delete;
		/** @brief 将任务异步投递到窗口所有者线程。 */
		static void dispatchTask(HWND window, std::function<void()> task);
		/** @brief 使用系统关联程序打开链接或文件。 */
		static void openLink(const std::wstring_view link);
		/** @brief 显示原生错误对话框。 */
		static void showError(const HWND window, const std::wstring_view message);

	private:
		/** @brief 处理原始输入、主题变更和窗口原生消息。 */
		static LRESULT CALLBACK				  procedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
		HWND								  mainWindow;
		std::function<void()>				  preferencesChanged;
		std::function<void(std::uint32_t)>	  keyPressed;
		std::function<void(std::string_view)> inputTrace;
		std::function<void(bool)>			  activationChanged;
		bool								  tracedMouseMove{};
		bool								  rawKeyboardRegistered{};
		std::array<bool, 256>				  keysDown{};
		std::shared_ptr<bool>				  alive{std::make_shared<bool>(true)};
	};
} // namespace flori_input::platform

#endif // FLORI_INPUT_PLATFORM_WINDOWS_PLATFORM_H
