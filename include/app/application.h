/**
 * @file application.h
 * @brief 声明应用启动参数和组合入口。
 */
#ifndef FLORI_INPUT_APP_APPLICATION_H
#define FLORI_INPUT_APP_APPLICATION_H

#include <functional>
#include <string_view>

namespace flori_input
{
	using TraceSink = std::function<void(std::string_view)>;

	struct ApplicationOptions final
	{
		bool preview{};
		bool diagnoseInput{};
		bool lifecycleHook{};
	};

	/**
	 * @brief 解析进程参数，识别预览、诊断与安装生命周期命令。
	 * @param argc 参数个数。
	 * @param argv 参数数组，仅在调用期间读取。
	 * @return 独立于参数数组生命周期的启动选项。
	 */
	[[nodiscard]] ApplicationOptions parseApplicationOptions(const int argc, char *const *const argv);
	/**
	 * @brief 装配应用服务并运行 Slint 事件循环。
	 * @param options 已解析的启动选项。
	 * @param trace 可选的诊断输出回调。
	 * @return 进程退出码；启动异常由调用方处理。
	 */
	int runApplication(const ApplicationOptions &options, const TraceSink &trace);
} // namespace flori_input

#endif // FLORI_INPUT_APP_APPLICATION_H
