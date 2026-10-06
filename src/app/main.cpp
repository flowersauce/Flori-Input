/**
 * @file main.cpp
 * @brief 提供进程入口、诊断日志和顶层异常边界。
 */
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <windows.h>
#include "app/app_paths.h"
#include "app/application.h"
#include "app/ui_resources.h"
#include "platform/windows_platform.h"

int main(const int argc, char **const argv) // NOLINT(misc-const-correctness)
{
	const auto options = flori_input::parseApplicationOptions(argc, argv);

	std::ofstream				 traceFile;
	std::mutex					 traceMutex;
	const flori_input::TraceSink trace = [&](const std::string_view message)
	{
		if (!options.diagnoseInput)
		{
			return;
		}

		const std::lock_guard lock(traceMutex);
		std::cerr << message << std::endl;
		if (traceFile.is_open())
		{
			traceFile << message << std::endl;
		}
	};

	try
	{
		const auto paths = flori_input::AppPaths::resolve(options.preview);
		const bool sharedInstalledInstance = !options.preview && paths.storageMode != flori_input::StorageMode::Portable;
		// 在打开日志前取得锁，避免重复启动截断正在运行实例的诊断文件。
		// 锁持续到事件循环退出且配置保存完成。
		const flori_input::platform::InstanceLock instanceLock(paths.configFile, sharedInstalledInstance);
		if (!instanceLock.acquired())
		{
			if (instanceLock.error() != ERROR_SHARING_VIOLATION)
			{
				flori_input::platform::WindowIntegration::showError(nullptr,
					L"无法打开配置目录或单实例锁。\nCannot open configuration directory or instance lock.");
			}
			return instanceLock.error() == ERROR_SHARING_VIOLATION ? 0 : 1;
		}
		if (options.diagnoseInput)
		{
			std::filesystem::create_directories(paths.logsDirectory);
			traceFile.open(paths.logsDirectory / "slint-input-trace.txt");
			if (!traceFile.is_open())
			{
				throw std::runtime_error("Cannot open the input diagnostic log");
			}
			trace("Starting input diagnostics");
		}

		return flori_input::runApplication(options, paths, trace);
	}
	catch (const std::exception &exception)
	{
		std::string message = std::string("Flori Input could not start: ") + exception.what();
		try
		{
			const flori_input::UiResources resources;
			message = resources.message(message, false);
		}
		catch (const std::exception &resourceError)
		{
			// 资源本身损坏时仍保留原始诊断，避免顶层异常处理再次失败。
			std::cerr << resourceError.what() << '\n';
		}
		MessageBoxA(nullptr, message.c_str(), "Flori Input", MB_OK | MB_ICONERROR);
		return 1;
	}
}
