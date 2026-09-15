/**
 * @file windows_input_backend.h
 * @brief 定义基于 SendInput 的 Windows 输入后端。
 */
#ifndef FLORI_INPUT_PLATFORM_WINDOWS_INPUT_BACKEND_H
#define FLORI_INPUT_PLATFORM_WINDOWS_INPUT_BACKEND_H

#include <functional>
#include <windows.h>
#include "core/program_executor.h"

namespace flori_input
{
	namespace platform
	{
		/** @brief 检查剧本中的所有移动坐标是否位于某块实际显示器。 */
		void validateVisibleMouseMoves(const Program &program);
	} // namespace platform
	class WindowsInputBackend final : public InputBackend
	{
	public:
		struct SendResult
		{
			UINT  accepted;
			DWORD error;
		};
		// 仅在提交输入前可以抛出异常；提交后须返回已接受的前缀与错误。
		using InputSender = std::function<SendResult(std::span<const INPUT>)>;
		/** @brief 固定键盘布局并选择原生事件发送器。 */
		explicit WindowsInputBackend(HKL layout, InputSender sender = {});
		/** @brief 在销毁前尝试释放全部已按下输入。 */
		~WindowsInputBackend() override;
		WindowsInputBackend(const WindowsInputBackend &)			= delete;
		WindowsInputBackend &operator=(const WindowsInputBackend &) = delete;
		/** @brief 按策略转换 Unicode 字符事件。 */
		Stroke character(char32_t value, const TextInputStrategy strategy) override;
		/** @brief 转换虚拟键事件。 */
		Stroke key(const std::uint16_t virtualKey) override;
		/** @brief 提交事件并记录成功的按下前缀。 */
		void send(std::span<const KeyboardEvent> events) override;
		/** @brief 尝试释放所有尚未配对的按下事件。 */
		bool releaseAll() noexcept override;
		/** @brief 查询成功提交的原生事件数量。 */
		std::size_t acceptedEvents() const
		{
			return accepted_;
		}
		/** @brief 查询清理时成功提交的抬起事件数量。 */
		std::size_t cleanupEvents() const
		{
			return cleanup_;
		}
		/** @brief 查询最近一次输入清理错误码。 */
		DWORD cleanupError() const
		{
			return cleanupError_;
		}

	private:
		/** @brief 根据当前布局获取虚拟键的扫描码事件。 */
		KeyboardEvent physical(const std::uint16_t virtualKey) const;
		/** @brief 发送原生事件并维护已按下事件集合。 */
		void					   submit(std::span<const KeyboardEvent> events, std::span<const INPUT> native);
		HKL						   layout_;
		InputSender				   sender_;
		std::vector<KeyboardEvent> held_;
		std::size_t				   accepted_{}, cleanup_{};
		DWORD					   cleanupError_{};
	};
} // namespace flori_input

#endif // FLORI_INPUT_PLATFORM_WINDOWS_INPUT_BACKEND_H
