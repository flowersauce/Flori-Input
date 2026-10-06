/**
 * @file app_paths.cpp
 * @brief 使用 Windows 包身份和已知目录解析配置、剧本与诊断日志路径。
 */
#include "app/app_paths.h"
#include <windows.h>
#include <appmodel.h>
#include <memory>
#include <optional>
#include <shlobj.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <winrt/Windows.Management.Core.h>
#include <winrt/Windows.Storage.h>

namespace flori_input
{
	namespace
	{
		/** @brief 配对释放 Windows 已知目录查询分配的字符串。 */
		struct TaskMemoryDeleter
		{
			void operator()(wchar_t *value) const noexcept
			{
				CoTaskMemFree(value);
			}
		};

		/** @brief 查询程序目录，与工作目录和安装盘符无关。 */
		std::filesystem::path currentExecutableDirectory()
		{
			std::vector<wchar_t> buffer(512);
			for (;;)
			{
				const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
				if (length == 0)
				{
					throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot locate the executable directory");
				}
				if (length < buffer.size())
				{
					return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
				}
				buffer.resize(buffer.size() * 2);
			}
		}

		/** @brief 只有明确无包身份时返回空值，其他查询错误不会切换数据模式。 */
		std::optional<std::wstring> packageFamilyName()
		{
			UINT32 length{};
			const LONG result = GetCurrentPackageFamilyName(&length, nullptr);
			if (result == APPMODEL_ERROR_NO_PACKAGE)
			{
				return std::nullopt;
			}
			if (result != ERROR_INSUFFICIENT_BUFFER)
			{
				throw std::system_error(result, std::system_category(), "Cannot query the package identity");
			}
			std::vector<wchar_t> name(length);
			const LONG readResult = GetCurrentPackageFamilyName(&length, name.data());
			if (readResult != ERROR_SUCCESS)
			{
				throw std::system_error(readResult, std::system_category(), "Cannot read the package identity");
			}
			return std::wstring(name.data());
		}

		/** @brief 只在查询包数据时初始化 WinRT，返回前释放，避免影响 Slint 的线程初始化。 */
		class RuntimeApartment final
		{
		public:
			RuntimeApartment()
			{
				winrt::init_apartment();
			}
			~RuntimeApartment()
			{
				winrt::uninit_apartment();
			}
			RuntimeApartment(const RuntimeApartment &) = delete;
			RuntimeApartment &operator=(const RuntimeApartment &) = delete;
		};

		/** @brief 普通安装使用当前用户的 LocalAppData，不读取环境变量拼接用户名。 */
		std::filesystem::path installedDataDirectory()
		{
			PWSTR value{};
			const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &value);
			const std::unique_ptr<wchar_t, TaskMemoryDeleter> directory(value);
			if (FAILED(result))
			{
				throw std::system_error(static_cast<int>(result), std::system_category(), "Cannot locate local application data");
			}
			return std::filesystem::path(directory.get()) / "Flori-Input";
		}
	} // namespace

	AppPaths AppPaths::resolve(const bool preview)
	{
		const auto directory = currentExecutableDirectory();
		const auto family = packageFamilyName();
		StorageMode mode{StorageMode::Installed};
		std::filesystem::path dataDirectory;
		std::filesystem::path logRoot;
		if (family)
		{
			try
			{
				const RuntimeApartment apartment;
				// 完整信任桌面进程按自身 PFN 访问数据，不要求 AppContainer 或 Windows App SDK。
				const auto data = winrt::Windows::Management::Core::ApplicationDataManager::CreateForPackageFamily(*family);
				dataDirectory = std::filesystem::path(data.LocalFolder().Path().c_str());
				logRoot = std::filesystem::path(data.LocalCacheFolder().Path().c_str());
			}
			catch (const winrt::hresult_error &error)
			{
				throw std::runtime_error("Cannot locate package application data: " + winrt::to_string(error.message()));
			}
			mode = StorageMode::Packaged;
		}
		else
		{
			const bool portable = std::filesystem::is_regular_file(directory / "portable.flag");
			mode = portable ? StorageMode::Portable : StorageMode::Installed;
			dataDirectory = portable ? directory : installedDataDirectory();
			logRoot = dataDirectory;
		}
		if (preview)
		{
			dataDirectory /= "preview";
			logRoot /= "preview";
		}
		return {.storageMode = mode,
				.executableDirectory = directory,
				.dataDirectory = dataDirectory,
				.configFile = dataDirectory / "config" / "config.jsonc",
				.scriptsDirectory = dataDirectory / "scripts",
				.logsDirectory = logRoot / "logs"};
	}
} // namespace flori_input
