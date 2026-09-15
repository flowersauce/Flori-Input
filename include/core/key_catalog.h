/**
 * @file key_catalog.h
 * @brief 声明虚拟键码与 key.* 翻译键的映射表。
 */
#ifndef FLORI_INPUT_CORE_KEY_CATALOG_H
#define FLORI_INPUT_CORE_KEY_CATALOG_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace flori_input
{
	extern const std::unordered_map<std::uint32_t, std::string> keyMap;
}

#endif // FLORI_INPUT_CORE_KEY_CATALOG_H
