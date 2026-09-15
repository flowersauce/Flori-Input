/**
 * @file windows_trigger_executor.h
 * @brief 声明高频触发器的 Windows 专用执行任务。
 */
#ifndef FLORI_INPUT_PLATFORM_WINDOWS_TRIGGER_EXECUTOR_H
#define FLORI_INPUT_PLATFORM_WINDOWS_TRIGGER_EXECUTOR_H

#include "core/execution_task.h"
#include "core/input_types.h"

#include <windows.h>

#include <functional>
#include <span>

namespace flori_input::platform
{
	using NativeInputSender = std::function<UINT(std::span<const INPUT>)>;

	// 创建独立的原生触发器任务；高频循环直接提交预构造的 INPUT 记录，不经过脚本执行器。
	/**
	 * @brief 预构造原生事件并生成高频触发器专用任务。
	 * @param config 已归一化的触发器设置。
	 * @param sender 注入事件的发送器；为空时使用 SendInput。
	 * @return 交由调度器执行的可取消任务。
	 */
	ExecutionTask makeWindowsTriggerTask(const InjectorConfig &config, NativeInputSender sender = {});
} // namespace flori_input::platform

#endif // FLORI_INPUT_PLATFORM_WINDOWS_TRIGGER_EXECUTOR_H
