/**
 * @file app_paths.h
 * @brief 集中解析不同发行方式的用户数据路径。
 */
#ifndef FLORI_INPUT_APP_APP_PATHS_H
#define FLORI_INPUT_APP_APP_PATHS_H

#include <filesystem>

namespace flori_input
{
	/** @brief 当前进程使用的数据存储方式。 */
	enum class StorageMode
	{
		Installed,
		Packaged,
		Portable
	};

	/** @brief 启动时解析的路径；解析本身不创建配置或日志文件。 */
	struct AppPaths final
	{
		StorageMode storageMode{StorageMode::Installed};
		std::filesystem::path executableDirectory;
		std::filesystem::path dataDirectory;
		std::filesystem::path configFile;
		std::filesystem::path scriptsDirectory;
		std::filesystem::path logsDirectory;

		/**
		 * @brief 按包身份、便携标记或用户目录解析路径，不依赖安装盘符。
		 * @param preview 是否将配置、剧本和日志放入独立预览目录。
		 * @return 当前用户和发行方式对应的路径。
		 * @throws std::exception 无法查询包身份或用户数据目录时抛出异常。
		 */
		[[nodiscard]] static AppPaths resolve(bool preview = false);
	};
} // namespace flori_input

#endif // FLORI_INPUT_APP_APP_PATHS_H
