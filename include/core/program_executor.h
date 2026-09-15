/**
 * @file program_executor.h
 * @brief 定义通用输入程序执行器及其平台抽象。
 */
#ifndef FLORI_INPUT_CORE_PROGRAM_EXECUTOR_H
#define FLORI_INPUT_CORE_PROGRAM_EXECUTOR_H

#include "core/execution_task.h"
#include "core/input_program.h"
#include "core/script_execution_limits.h"

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>

namespace flori_input
{
	class InputError : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};
	struct KeyboardEvent
	{
		enum class Kind
		{
			ScanCode,
			VirtualKey,
			MouseButton,
			MouseMove,
			Wheel
		};
		std::uint16_t code{}; // 扫描码、Unicode 码元或鼠标按键编号。
		bool		  unicode{};
		bool		  extended{};
		bool		  up{};
		Kind		  kind{Kind::ScanCode};
		int			  x{}, y{}; // 鼠标位置；滚轮事件使用 y 表示步数。
		friend bool	  operator==(const KeyboardEvent &, const KeyboardEvent &) = default;
	};
	struct Stroke
	{
		std::vector<KeyboardEvent> down;
		std::vector<KeyboardEvent> up;
	};
	class InputBackend
	{
	public:
		virtual ~InputBackend() = default;
		/** @brief 将一个 Unicode 标量转换为按下与抬起事件。 */
		virtual Stroke character(char32_t value, TextInputStrategy strategy) = 0;
		/** @brief 将虚拟键转换为按下与抬起事件。 */
		virtual Stroke key(std::uint16_t virtualKey) = 0;
		// 实现须追踪成功提交的按下事件，包括部分提交的情况。
		/** @brief 提交事件并记录已成功按下的输入。 */
		virtual void send(std::span<const KeyboardEvent> events) = 0;
		// 必须尝试释放全部未配对输入；返回 false 表示清理不完整。
		/** @brief 尝试释放全部未配对按下事件。 */
		virtual bool releaseAll() noexcept = 0;
	};
	class ExecutionClock
	{
	public:
		using TimePoint			  = std::chrono::steady_clock::time_point;
		virtual ~ExecutionClock() = default;
		/** @brief 获取单调时钟当前时刻。 */
		virtual TimePoint now() const = 0;
		/** @brief 等待截止时间；取消时返回 false。 */
		virtual bool waitUntil(TimePoint deadline, std::stop_token stop) = 0;
	};
	class SteadyExecutionClock final : public ExecutionClock
	{
	public:
		/** @brief 使用 steady_clock 读取当前时刻。 */
		TimePoint now() const override;
		/** @brief 使用可取消的稳态时钟等待截止时间。 */
		bool waitUntil(TimePoint deadline, std::stop_token stop) override;
	};
	struct ExecutionResult
	{
		std::size_t completedActions{};
		bool		cancelled{};
	};
	// 同步执行；调用方负责工作线程生命周期，并独占后端与时钟。
	/**
	 * @brief 同步执行冻结后的程序并在退出前清理按下事件。
	 * @param program 已完成结构校验的输入程序。
	 * @param backend 独占使用的输入后端。
	 * @param clock 执行期间使用的单调时钟。
	 * @param stop 外部取消令牌。
	 * @param scriptLimits 事件剧本专用限制；留空时保持模拟粘贴的原有执行语义。
	 * @return 已完成动作数量与取消状态。
	 */
	ExecutionResult executeProgram(const Program					   &program,
								   InputBackend						   &backend,
								   ExecutionClock					   &clock,
								   const std::stop_token			   &stop		 = {},
								   std::optional<ScriptExecutionLimits> scriptLimits = {});

	using InputBackendFactory = std::function<std::unique_ptr<InputBackend>(std::stop_token)>;
	/** @brief 将程序及平台后端工厂封装为可调度任务，并在指定时预校验剧本限制。 */
	ExecutionTask makeProgramExecutionTask(Program program, InputBackendFactory factory, std::optional<ScriptExecutionLimits> scriptLimits = {});
} // namespace flori_input

#endif // FLORI_INPUT_CORE_PROGRAM_EXECUTOR_H
