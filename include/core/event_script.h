/**
 * @file event_script.h
 * @brief 声明事件剧本解析入口和解析错误类型。
 */
#ifndef FLORI_INPUT_CORE_EVENT_SCRIPT_H
#define FLORI_INPUT_CORE_EVENT_SCRIPT_H
#include <stdexcept>
#include "core/input_program.h"

namespace flori_input
{
	class EventScriptError final : public std::runtime_error
	{
	public:
		/** @brief 将解析位置与错误信息组合为诊断文本。 */
		EventScriptError(std::size_t line, std::size_t column, std::string message)
			: std::runtime_error(std::to_string(line) + ":" + std::to_string(column) + " " + message)
		{
		}
	};
	/** @brief 解析 UTF-8 事件剧本并生成已校验的输入程序。 */
	Program eventScriptProgram(std::string_view source);
} // namespace flori_input

#endif // FLORI_INPUT_CORE_EVENT_SCRIPT_H
