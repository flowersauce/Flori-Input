/**
 * @file execution_task.h
 * @brief 定义可取消执行任务及其结果。
 */
#ifndef FLORI_INPUT_CORE_EXECUTION_TASK_H
#define FLORI_INPUT_CORE_EXECUTION_TASK_H

#include <functional>
#include <stdexcept>
#include <stop_token>

namespace flori_input
{
	using ExecutionTask = std::function<void(std::stop_token)>;

	// 停止时未能释放全部注入输入；调度器须阻止后续任务，避免按键或鼠标按钮保持按下。
	class CleanupError : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};
} // namespace flori_input

#endif // FLORI_INPUT_CORE_EXECUTION_TASK_H
