/**
 * @file paste_input.cpp
 * @brief 实现剪贴板读取及目标窗口约束输入后端。
 */
#include "platform/paste_input.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <thread>
#include <utility>

namespace flori_input::platform
{
	std::u16string clipboardTextSnapshot()
	{
		for (int attempt = 0; !OpenClipboard(nullptr); ++attempt)
		{
			if (attempt == 9)
			{
				throw InputError("[[clipboard.busy]]");
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		struct Close
		{
			~Close()
			{
				CloseClipboard();
			}
		};
		const Close close;
		if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
		{
			throw InputError("[[clipboard.no_text]]");
		}
		const HANDLE data = GetClipboardData(CF_UNICODETEXT);
		if (!data)
		{
			throw InputError("[[clipboard.read_failed]]");
		}
		const auto	size = GlobalSize(data) / sizeof(wchar_t);
		const auto *text = static_cast<const wchar_t *>(GlobalLock(data));
		if (!text)
		{
			throw InputError("[[clipboard.lock_failed]]");
		}
		struct Unlock
		{
			HANDLE data;
			~Unlock()
			{
				GlobalUnlock(data);
			}
		};
		const Unlock unlock{data};
		const auto	*end = std::find(text, text + size, L'\0');
		if (end == text || end == text + size)
		{
			throw InputError("[[clipboard.invalid_text]]");
		}
		return std::u16string(text, end);
	}

	/** @brief 将文本复制为 CF_UNICODETEXT，并在提交成功后转移全局内存所有权。 */
	void writeClipboardText(const HWND owner, const std::wstring_view text)
	{
		constexpr auto failure = "[[clipboard.write_failed]]";
		if (!owner || text.size() > std::numeric_limits<SIZE_T>::max() / sizeof(wchar_t) - 1)
		{
			throw InputError(failure);
		}
		const auto									freeMemory = [](const HGLOBAL value) { GlobalFree(value); };
		std::unique_ptr<void, decltype(freeMemory)> memory{GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t)), freeMemory};
		if (!memory)
		{
			throw InputError(failure);
		}
		auto *const destination = static_cast<wchar_t *>(GlobalLock(memory.get()));
		if (!destination)
		{
			throw InputError(failure);
		}
		std::copy(text.begin(), text.end(), destination);
		destination[text.size()] = L'\0';
		GlobalUnlock(memory.get());

		for (int attempt = 0; !OpenClipboard(owner); ++attempt)
		{
			if (attempt == 9)
			{
				throw InputError("[[clipboard.busy]]");
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		struct Close
		{
			~Close()
			{
				CloseClipboard();
			}
		};
		const Close close;
		if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, memory.get()))
		{
			throw InputError(failure);
		}
		memory.release();
	}
	namespace
	{
		class TargetBackend final : public InputBackend
		{
		public:
			/** @brief 固定目标窗口与输入布局。 */
			TargetBackend(const HWND target, const HKL layout, WindowsInputBackend::InputSender sender)
				: target_(target)
				, backend_(layout, std::move(sender))
			{
			}
			/** @brief 将字符转换委托给 Windows 输入后端。 */
			Stroke character(const char32_t ch, const TextInputStrategy strategy) override
			{
				return backend_.character(ch, strategy);
			}
			/** @brief 将按键转换委托给 Windows 输入后端。 */
			Stroke key(const std::uint16_t vk) override
			{
				return backend_.key(vk);
			}
			/** @brief 尝试释放全部未配对输入。 */
			bool releaseAll() noexcept override
			{
				return backend_.releaseAll();
			}
			/** @brief 提交输入前检查焦点，并在首次按键或鼠标按下前检查外部修饰键。 */
			void send(std::span<const KeyboardEvent> events) override
			{
				if (std::ranges::any_of(events, [](const auto &event) { return !event.up; }))
				{
					if (GetForegroundWindow() != target_)
					{
						throw InputError("[[paste.target_changed]]");
					}
					const bool pressesButton =
						std::ranges::any_of(events,
											[](const KeyboardEvent &event)
											{ return !event.up && event.kind != KeyboardEvent::Kind::MouseMove && event.kind != KeyboardEvent::Kind::Wheel; });
					if (pressesButton && !checkedModifiers_)
					{
						for (const int vk: {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN})
						{
							if (GetAsyncKeyState(vk) & 0x8000)
							{
								throw InputError("[[paste.release_modifiers]]");
							}
						}
						checkedModifiers_ = true;
					}
				}
				backend_.send(events);
			}

		private:
			HWND				target_;
			WindowsInputBackend backend_;
			bool				checkedModifiers_{};
		};
	} // namespace
	InputBackendFactory pasteBackendFactory(HWND target, std::uint32_t trigger, WindowsInputBackend::InputSender sender)
	{
		const auto layout = GetKeyboardLayout(GetWindowThreadProcessId(target, nullptr));
		return [target, layout, trigger, sender = std::move(sender)](const std::stop_token &stop) -> std::unique_ptr<InputBackend>
		{
			SteadyExecutionClock clock;
			const auto			 deadline = clock.now() + std::chrono::seconds(2);
			while (GetAsyncKeyState(static_cast<int>(trigger)) & 0x8000)
			{
				if (!clock.waitUntil(clock.now() + std::chrono::milliseconds(5), stop))
				{
					return {};
				}
				if (clock.now() >= deadline)
				{
					throw InputError("[[paste.release_hotkey]]");
				}
			}
			return std::make_unique<TargetBackend>(target, layout, sender);
		};
	}
} // namespace flori_input::platform
