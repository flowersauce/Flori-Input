/**
 * @file input_router.h
 * @brief 定义按当前页面和任务所有者分派热键的路由器。
 */
#ifndef FLORI_INPUT_CORE_INPUT_ROUTER_H
#define FLORI_INPUT_CORE_INPUT_ROUTER_H

#include <cstdint>
#include "core/feature.h"

namespace flori_input
{
	class InputRouter final
	{
	public:
		enum class Action
		{
			Ignore,
			Capture,
			Start,
			Stop
		};
		/** @brief 更新当前页面编号。 */
		void selectPage(const int page)
		{
			page_ = page;
		}
		/** @brief 查询当前页面对应的功能。 */
		Feature feature() const;
		/** @brief 按捕获状态、任务所有者和当前热键决定处理动作。 */
		Action route(Feature capture, Feature owner, std::uint32_t key, std::uint32_t hotkey) const;

	private:
		int page_{};
	};
} // namespace flori_input

#endif // FLORI_INPUT_CORE_INPUT_ROUTER_H
