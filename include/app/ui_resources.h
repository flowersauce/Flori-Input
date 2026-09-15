/**
 * @file ui_resources.h
 * @brief 定义内置主题和语言资源服务。
 */
#ifndef FLORI_INPUT_APP_UI_RESOURCES_H
#define FLORI_INPUT_APP_UI_RESOURCES_H
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace flori_input
{
	class UiResources final
	{
	public:
		/** @brief 解析编译进程序的主题和语言资源。 */
		UiResources();
		/** @brief 按键查询指定语言的界面文本。 */
		const char *text(std::string_view key, bool chinese) const;
		/** @brief 按 key.* 语义键查询本地化后的按键名称。 */
		std::string keyLabel(std::string_view name, bool chinese) const;
		/** @brief 翻译诊断中的 [[domain.key]] 标记，保留标记外的动态数据。 */
		std::string message(std::string_view source, bool chinese) const;
		/** @brief 查询深色或浅色主题颜色。 */
		std::uint32_t color(std::string_view key, bool dark) const;

	private:
		using Dictionary = std::map<std::string, std::string, std::less<>>;
		struct Language
		{
			Dictionary strings, diagnostics, keys;
		};
		std::array<Language, 2>											 languages_;
		std::array<std::map<std::string, std::uint32_t, std::less<>>, 2> themes_;
	};
} // namespace flori_input

#endif // FLORI_INPUT_APP_UI_RESOURCES_H
