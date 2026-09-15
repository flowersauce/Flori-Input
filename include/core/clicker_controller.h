/**
 * @file clicker_controller.h
 * @brief 定义触发器配置、捕获和运行状态机。
 */
#ifndef FLORI_INPUT_CORE_CLICKER_CONTROLLER_H
#define FLORI_INPUT_CORE_CLICKER_CONTROLLER_H

#include <functional>
#include <string_view>

#include "core/input_types.h"

namespace flori_input
{
	// 所有方法和注入操作均在所有者（界面）线程运行。
	// 应用须将调度器完成事件转发至 executionFinished()。
	// 适配层负责停止并回收注入线程，再销毁本对象；销毁后须丢弃排队的完成回调。
	// 启动操作若保留配置须复制；每次运行结束必须且仅能通知一次，随后才可接受下一次运行。
	// 本类不持有工作线程或输入钩子。
	class ClickerController final
	{
	public:
		enum class Capture
		{
			None,
			StartHotkey,
			CustomKey,
			Coordinate,
			CoordinateSelection
		};
		enum class RunState
		{
			Idle,
			Running,
			Stopping
		};
		struct Config
		{
			std::uint32_t  startHotkey{DefaultStartHotkey};
			InjectorConfig injector;
		};
		using Start = std::function<bool(const InjectorConfig &)>;
		using Stop	= std::function<void()>;

		/** @brief 注入触发器启动与停止操作。 */
		ClickerController(Start start, Stop stop);
		/** @brief 获取归一化后的触发器配置。 */
		const Config &config() const
		{
			return configValue;
		}
		/** @brief 获取当前按键或坐标捕获状态。 */
		Capture capture() const
		{
			return captureValue;
		}
		/** @brief 获取触发器运行状态。 */
		RunState runState() const
		{
			return runStateValue;
		}
		/** @brief 判断执行任务是否仍处于运行或停止中。 */
		bool running() const
		{
			return runStateValue != RunState::Idle;
		}
		/** @brief 判断当前配置是否允许启动。 */
		bool canStart() const;
		/** @brief 获取用于界面展示的状态文本。 */
		std::string_view statusText() const;

		/** @brief 应用新配置，运行中拒绝更新。 */
		bool setConfig(const Config &config);
		/** @brief 对配置值进行约束和冲突归一化。 */
		static Config normalizedConfig(Config config);
		/** @brief 开始指定类型的输入捕获。 */
		void beginCapture(Capture capture);
		/** @brief 放弃任一坐标捕获并恢复常态。 */
		void cancelCoordinateCapture();
		/** @brief 保存选中的屏幕坐标。 */
		void finishCoordinateCapture(ScreenPoint point);
		/** @brief 处理已路由到触发器的按键。 */
		void handleKeyPressed(std::uint32_t key);
		/** @brief 根据当前状态启动或停止触发器。 */
		void toggleRunning();
		/** @brief 请求停止运行中的触发器。 */
		void stop();
		/** @brief 接收调度器的执行完成通知。 */
		void executionFinished();

	private:
		Config	 configValue;
		Capture	 captureValue{Capture::None};
		RunState runStateValue{RunState::Idle};
		bool	 invalidCaptureKey{};
		Start	 startOperation;
		Stop	 stopOperation;
	};
} // namespace flori_input

#endif // FLORI_INPUT_CORE_CLICKER_CONTROLLER_H
