/**
 * @file ui_resources.cpp
 * @brief 解析并查询编译进程序的主题和语言资源。
 */
#include "app/ui_resources.h"
#include <array>
#include <charconv>
#include <stdexcept>
#include <string>
#include <windows.h>
#include "core/json.h"
#include "ui_resource_defaults.h"

namespace flori_input
{
	namespace
	{
		/** @brief 验证并取得 JSON 对象。 */
		const json::Value::object_t &object(const json::Value &value)
		{
			if (!value.is_object())
			{
				throw std::runtime_error("Expected object");
			}
			return value.get_object();
		}
		/** @brief 验证资源文本的类型、长度和 UTF-8 编码。 */
		const std::string &string(const json::Value &value)
		{
			const auto result = value.is_string() ? &value.get_string() : nullptr;
			if (!result || result->size() > 4096 || result->find('\0') != std::string::npos)
			{
				throw std::runtime_error("Expected text of at most 4096 bytes without NUL");
			}
			if (!result->empty() && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, result->data(), static_cast<int>(result->size()), nullptr, 0))
			{
				throw std::runtime_error("Invalid UTF-8 text");
			}
			return *result;
		}
		/** @brief 将十六进制颜色转为 Slint 所需的 ARGB 值。 */
		std::uint32_t rgba(const std::string_view text)
		{
			if ((text.size() != 7 && text.size() != 9) || text.front() != '#')
			{
				throw std::runtime_error("Expected #RRGGBB or #RRGGBBAA");
			}
			std::uint32_t value{};
			if (const auto [ptr, ec] = std::from_chars(text.data() + 1, text.data() + text.size(), value, 16);
				ec != std::errc{} || ptr != text.data() + text.size())
			{
				throw std::runtime_error("Invalid color");
			}
			return text.size() == 7 ? 0xff000000U | value : (value >> 8) | (value << 24);
		}
	} // namespace

	UiResources::UiResources()
	{
		constexpr std::array themeSources{defaults::dark, defaults::light};
		constexpr std::array languageSources{defaults::zh, defaults::en};
		for (std::size_t index = 0; index < themeSources.size(); ++index)
		{
			const auto theme = json::parse<true>(themeSources[index]);
			for (const auto &[key, value]: object(theme))
			{
				themes_[index].emplace(key, rgba(string(value)));
			}
			const auto language = json::parse<true>(languageSources[index]);
			for (const auto &[section, values]: object(language))
			{
				if (section != "strings" && section != "diagnostics" && section != "keys")
				{
					throw std::runtime_error("Unknown embedded language section: " + section);
				}
				auto &table = section == "strings" ? languages_[index].strings : section == "keys" ? languages_[index].keys : languages_[index].diagnostics;
				for (const auto &[key, value]: object(values))
				{
					table.emplace(key, string(value));
				}
			}
		}
	}
	const char *UiResources::text(const std::string_view key, const bool chinese) const
	{
		const auto &table = languages_[chinese ? 0 : 1].strings;
		const auto	found = table.find(key);
		return found == table.end() ? "[missing translation]" : found->second.c_str();
	}
	std::string UiResources::keyLabel(const std::string_view name, const bool chinese) const
	{
		const auto &table = languages_[chinese ? 0 : 1].keys;
		const auto	found = table.find(name);
		return found == table.end() ? std::string(name) : found->second;
	}
	std::uint32_t UiResources::color(const std::string_view key, const bool dark) const
	{
		const auto &table = themes_[dark ? 0 : 1];
		const auto	found = table.find(key);
		return found == table.end() ? 0xffff00ffU : found->second;
	}
	std::string UiResources::message(const std::string_view source, const bool chinese) const
	{
		const auto &table = languages_[chinese ? 0 : 1].diagnostics;
		// 只解析显式语义键标记；行列号、坐标和原生错误信息按原样保留。
		// 不递归解析译文，避免译文中的字符被再次解释为标记。
		std::string result;
		for (std::size_t pos = 0; pos < source.size();)
		{
			const auto begin = source.find("[[", pos);
			if (begin == std::string_view::npos)
			{
				result.append(source.substr(pos));
				break;
			}
			const auto end = source.find("]]", begin + 2);
			if (end == std::string_view::npos)
			{
				result.append(source.substr(pos));
				break;
			}
			result.append(source.substr(pos, begin - pos));
			const auto key = source.substr(begin + 2, end - begin - 2);
			const auto found = table.find(key);
			result += found == table.end() ? "[missing translation]" : found->second;
			pos = end + 2;
		}
		return result;
	}
} // namespace flori_input
