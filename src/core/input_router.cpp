/**
 * @file input_router.cpp
 * @brief 实现页面感知的热键动作路由。
 */
#include "core/input_router.h"

namespace flori_input
{
	Feature InputRouter::feature() const
	{
		return page_ == 0 ? Feature::Clicker : page_ == 3 ? Feature::Paste : page_ == 4 ? Feature::Script : Feature::None;
	}

	InputRouter::Action InputRouter::route(const Feature capture, const Feature owner, const std::uint32_t key, const std::uint32_t hotkey) const
	{
		const auto current = feature();
		if (current == Feature::None)
		{
			return Action::Ignore;
		}
		if (capture != Feature::None)
		{
			return capture == current ? Action::Capture : Action::Ignore;
		}
		if (key != hotkey)
		{
			return Action::Ignore;
		}
		if (owner != Feature::None)
		{
			return owner == current ? Action::Stop : Action::Ignore;
		}
		return Action::Start;
	}
} // namespace flori_input
