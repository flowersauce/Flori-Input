/**
 * @file main.cpp
 * @brief 提供进程入口、诊断日志和顶层异常边界。
 */
#include <exception>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <windows.h>
#include "app/app_config.h"
#include "app/application.h"
#include "app/ui_resources.h"

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
		if (options.diagnoseInput)
		{
			traceFile.open(flori_input::AppConfig::executableDirectory() / "slint-input-trace.txt");
			if (!traceFile.is_open())
			{
				trace("Cannot open trace file; using stderr only");
			}
			trace("Starting input diagnostics");
		}

		return flori_input::runApplication(options, trace);
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
