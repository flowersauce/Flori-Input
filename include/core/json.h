/**
 * @file json.h
 * @brief 封装项目统一使用的 Glaze JSONC 读写策略。
 */
#ifndef FLORI_INPUT_CORE_JSON_H
#define FLORI_INPUT_CORE_JSON_H

#include <glaze/json/generic.hpp>
#include <glaze/json/read.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace flori_input::json
{
	using Value = glz::generic_sorted;

	template<bool Comments = false>
	struct ReadOptions : glz::opts
	{
		bool validate_trailing_whitespace = true;
		constexpr ReadOptions()
		{
			comments		= Comments;
			null_terminated = false;
		}
	};

	struct WriteOptions : glz::opts
	{
		std::uint8_t indentation_width = 4;
		constexpr WriteOptions()
		{
			prettify		  = true;
			skip_null_members = false;
		}
	};

	/** @brief 解析 UTF-8 JSON；Comments 为真时允许 JSONC 注释。 */
	template<bool Comments = false>
	Value parse(std::string_view source)
	{
		if (source.starts_with("\xef\xbb\xbf"))
		{
			source.remove_prefix(3);
		}
		Value value;
		if (const auto error = glz::read<ReadOptions<Comments>{}>(value, source))
		{
			throw std::runtime_error(glz::format_error(error, source));
		}
		return value;
	}

	/** @brief 将通用 JSON 值序列化为格式化文本。 */
	inline std::string write(const Value &value)
	{
		std::string result;
		if (const auto error = glz::write<WriteOptions{}>(value, result))
		{
			throw std::runtime_error(glz::format_error(error));
		}
		return result;
	}

	/** @brief 在对象中查找字段；非对象或字段不存在时返回空指针。 */
	inline const Value *find(const Value &value, const std::string_view key)
	{
		if (!value.is_object())
		{
			return nullptr;
		}
		const auto &object = value.get_object();
		const auto	entry  = object.find(key);
		return entry == object.end() ? nullptr : &entry->second;
	}
} // namespace flori_input::json

#endif // FLORI_INPUT_CORE_JSON_H
