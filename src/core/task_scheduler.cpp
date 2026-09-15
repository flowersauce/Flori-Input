/**
 * @file task_scheduler.cpp
 * @brief 实现应用级单任务调度与安全回收。
 */
#include "core/task_scheduler.h"
#include <exception>
#include <utility>

namespace flori_input
{
	TaskScheduler::~TaskScheduler()
	{
		shutdown();
	}

	bool TaskScheduler::start(const Feature feature, ExecutionTask task)
	{
		if (feature == Feature::None || !task || active() || blocked_)
		{
			return false;
		}
		error_.clear();
		workerError_.clear();
		workerCleanupFailed_ = false;
		done_.store(false, std::memory_order_relaxed);
		try
		{
			worker_ = std::jthread(
				[this, task = std::move(task)](const std::stop_token &stop)
				{
					try
					{
						task(stop);
					}
					catch (const CleanupError &error)
					{
						workerError_		 = error.what();
						workerCleanupFailed_ = true;
					}
					catch (const std::exception &error)
					{
						workerError_ = error.what();
					}
					catch (...)
					{
						workerError_ = "[[execution.unknown_failure]]";
					}
					done_.store(true, std::memory_order_release);
				});
			owner_ = feature;
			state_ = State::Running;
			return true;
		}
		catch (const std::exception &error)
		{
			error_ = error.what();
			return false;
		}
	}

	void TaskScheduler::stop()
	{
		if (!active())
		{
			return;
		}
		state_ = State::Stopping;
		worker_.request_stop();
	}

	Feature TaskScheduler::poll()
	{
		if (!active() || !done_.load(std::memory_order_acquire))
		{
			return Feature::None;
		}
		worker_.join();
		error_				 = std::move(workerError_);
		blocked_			 = workerCleanupFailed_;
		const auto completed = owner_;
		owner_				 = Feature::None;
		state_				 = State::Idle;
		return completed;
	}

	void TaskScheduler::shutdown()
	{
		stop();
		if (worker_.joinable())
		{
			worker_.join();
		}
		owner_ = Feature::None;
		state_ = State::Idle;
	}
} // namespace flori_input
