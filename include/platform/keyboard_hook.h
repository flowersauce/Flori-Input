/**
 * @file keyboard_hook.h
 * @brief 定义全局鼠标输入钩子及按键回调适配器。
 */
#ifndef FLORI_INPUT_PLATFORM_KEYBOARD_HOOK_H
#define FLORI_INPUT_PLATFORM_KEYBOARD_HOOK_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <windows.h>
#include "core/input_types.h"

namespace flori_input
{
	class KeyboardHook final
	{
	public:
		using KeyCallback			 = std::function<void(std::uint32_t keyCode)>;
		using CoordinateCallback	 = std::function<void(ScreenPoint point, bool picked)>;
		using CoordinateMoveCallback = std::function<void(ScreenPoint point)>;

		/** @brief 保存鼠标按键与坐标选取回调。 */
		explicit KeyboardHook(KeyCallback keyCallback, CoordinateCallback coordinateCallback = {}, CoordinateMoveCallback moveCallback = {});
		/** @brief 卸载钩子并释放全局所有权。 */
		~KeyboardHook();

		KeyboardHook(const KeyboardHook &)			  = delete;
		KeyboardHook &operator=(const KeyboardHook &) = delete;
		KeyboardHook(KeyboardHook &&)				  = delete;
		KeyboardHook &operator=(KeyboardHook &&)	  = delete;

		/** @brief 安装当前实例的低级鼠标钩子。 */
		bool install();
		/** @brief 卸载已安装的低级鼠标钩子。 */
		void uninstall();
		/** @brief 查询钩子是否已安装。 */
		bool isInstalled() const;
		/** @brief 仅在坐标选取期间截获左右键及其抬起事件。 */
		void setCoordinateCaptureActive(bool active);

	private:
		/** @brief 接收全局鼠标消息并过滤本程序注入事件。 */
		static LRESULT CALLBACK mouseHookCallback(int code, WPARAM message, LPARAM eventData);

		/** @brief 清除当前实例的全局钩子所有权。 */
		void releaseOwnership();
		/** @brief 将受支持的鼠标事件分派给回调。 */
		void dispatch(std::uint32_t keyCode) const noexcept;
		/** @brief 将全局屏幕坐标分派给坐标选取回调。 */
		void dispatchCoordinate(ScreenPoint point, bool picked) const noexcept;
		/** @brief 在鼠标移动事件入队前同步定位坐标窗口。 */
		void dispatchCoordinateMove(ScreenPoint point) const noexcept;

		static std::atomic<KeyboardHook *> activeHook;

		KeyCallback			   keyCallback;
		CoordinateCallback	   coordinateCallback;
		CoordinateMoveCallback coordinateMoveCallback;
		std::atomic<bool>	   coordinateCaptureActive{};
		std::atomic<bool>	   suppressLeftUp{};
		std::atomic<bool>	   suppressRightUp{};
		HHOOK				   mouseHook{};
	};
} // namespace flori_input

#endif // FLORI_INPUT_PLATFORM_KEYBOARD_HOOK_H
