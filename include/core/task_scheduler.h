/**
 * @file task_scheduler.h
 * @brief 定义应用级单任务调度与生命周期管理。
 */
#ifndef FLORI_INPUT_CORE_TASK_SCHEDULER_H
#define FLORI_INPUT_CORE_TASK_SCHEDULER_H

#include "core/execution_task.h"
#include "core/feature.h"

#include <atomic>
#include <string>
#include <thread>

namespace flori_input
{
	// 持有应用唯一的执行槽；任务保留各自的执行模型，本类负责生命周期、取消和互斥。
	class TaskScheduler final
	{
	public:
		enum class State
		{
			Idle,
			Running,
			Stopping
		};

		/** @brief 停止并回收尚未结束的工作线程。 */
		~TaskScheduler();
		/**
		 * @brief 在唯一执行槽空闲时启动指定功能任务。
		 * @param feature 拥有执行槽的功能。
		 * @param task 可取消任务，启动成功后转移所有权。
		 * @return 成功占用执行槽时为 true。
		 */
		bool start(const Feature feature, ExecutionTask task);
		/** @brief 请求当前任务取消，不阻塞调用线程。 */
		void stop();
		/** @brief 回收已完成任务并返回其功能标识。 */
		Feature poll();
		/** @brief 终止调度器并同步回收工作线程。 */
		void shutdown();

		/** @brief 查询执行槽是否仍被占用。 */
		bool active() const
		{
			return state_ != State::Idle;
		}
		/** @brief 查询是否因输入清理失败而拒绝后续任务。 */
		bool blocked() const
		{
			return blocked_;
		}
		/** @brief 查询当前任务的功能所有者。 */
		Feature owner() const
		{
			return owner_;
		}
		/** @brief 查询任务生命周期状态。 */
		State state() const
		{
			return state_;
		}
		/** @brief 查询最近一次任务的错误描述。 */
		const std::string &error() const
		{
			return error_;
		}

	private:
		State			  state_{State::Idle};
		Feature			  owner_{Feature::None};
		std::string		  error_;
		std::atomic<bool> done_{};
		std::string		  workerError_;
		bool			  blocked_{};
		bool			  workerCleanupFailed_{};
		std::jthread	  worker_;
	};
} // namespace flori_input

#endif // FLORI_INPUT_CORE_TASK_SCHEDULER_H
