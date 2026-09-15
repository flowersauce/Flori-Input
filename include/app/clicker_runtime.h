/**
 * @file clicker_runtime.h
 * @brief 定义触发器控制器与平台输入钩子的运行时适配器。
 */
#ifndef FLORI_INPUT_APP_CLICKER_RUNTIME_H
#define FLORI_INPUT_APP_CLICKER_RUNTIME_H

#include <functional>
#include <memory>

#include "core/clicker_controller.h"

namespace flori_input
{
	// 在界面所有者线程构造、访问和销毁；分发器须线程安全，仅排队而不内联执行，并在该线程送达。
	// shutdown() 返回前，分发器接受任务时不得抛出异常。
	class ClickerRuntime final
	{
	public:
		using Task				  = std::function<void()>;
		using Dispatcher		  = std::function<void(Task)>;
		using Changed			  = std::function<void(const ClickerController &)>;
		using KeyCallback		  = std::function<void(std::uint32_t)>;
		using CoordinateMove	  = std::function<void(ScreenPoint)>;
		using CoordinateSelection = std::function<void(ScreenPoint)>;
		// 所有者线程分发之前，在钩子或事件源线程运行。
		using KeyObserved = std::function<void(std::uint32_t)>;
		// 在所有者线程运行；默认路由保留仅处理触发器的行为，应用可替换为按功能区分的路由。
		using KeyRoute = std::function<void(ClickerController &, std::uint32_t)>;

		/** @brief 注入线程调度、状态通知、热键路由及触发器操作。 */
		explicit ClickerRuntime(Dispatcher				 dispatcher,
								Changed					 changed,
								KeyObserved				 keyObserved,
								KeyRoute				 keyRoute,
								ClickerController::Start start,
								ClickerController::Stop	 stop,
								CoordinateMove			 coordinateMove = {});
		/** @brief 确保输入钩子和待执行任务安全收尾。 */
		~ClickerRuntime();
		ClickerRuntime(const ClickerRuntime &)			  = delete;
		ClickerRuntime &operator=(const ClickerRuntime &) = delete;
		ClickerRuntime(ClickerRuntime &&)				  = delete;
		ClickerRuntime &operator=(ClickerRuntime &&)	  = delete;

		/** @brief 获取仅供所有者线程读取的控制器状态。 */
		const ClickerController &controller() const;
		// Operation、Changed 和 KeyRoute 不得保留对控制器的引用。
		/** @brief 在所有者线程修改控制器并通知状态变化。 */
		void update(const std::function<void(ClickerController &)> &operation) const;
		/** @brief 安装平台输入钩子。 */
		bool installHooks() const;
		/** @brief 查询平台输入钩子安装状态。 */
		bool hooksInstalled() const;
		/** @brief 切换鼠标钩子的坐标选取模式。 */
		void setCoordinateCaptureActive(bool active) const;
		/** @brief 设置复制坐标模式的确认回调；在所有者线程调用。 */
		void setCoordinateSelectionHandler(CoordinateSelection handler) const;
		// 返回的回调可由任意线程安全调用，销毁后亦然。
		/** @brief 获取可跨线程调用的按键回调。 */
		KeyCallback keyCallback() const;
		/** @brief 使此前排队的热键事件失效。 */
		void invalidateKeyEvents() const;
		// 终结且幂等：使排队任务失效、卸载钩子并请求停止。
		/** @brief 幂等关闭运行时并丢弃未送达事件。 */
		void shutdown() const;

	private:
		struct State;
		std::shared_ptr<State> state;
	};
} // namespace flori_input

#endif // FLORI_INPUT_APP_CLICKER_RUNTIME_H
