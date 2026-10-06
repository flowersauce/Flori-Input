<p align="center"><img src="resources/icons/Flori-Input_transparent_dynamic.svg" alt="Flori Input" width="120"></p>

<h1 align="center">Flori Input</h1>

<p align="center">面向 Windows 的键鼠输入工具，支持重复输入、模拟粘贴和事件剧本。由 FSClicker 演进而来，并基于 Slint UI 重构。</p>

<p align="center">
  <img alt="Windows" src="https://img.shields.io/badge/Windows-0078D4?style=flat-square&logo=windows&logoColor=white">
  <img alt="C++23" src="https://img.shields.io/badge/C%2B%2B-23-00599C?style=flat-square&logo=cplusplus&logoColor=white">
  <img alt="Slint" src="https://img.shields.io/badge/Slint-2379F4?style=flat-square&logo=Slint&logoColor=white">
  <img alt="License" src="https://img.shields.io/badge/License-MIT-2DA44E?style=flat-square">
</p>

<p align="center">
  中文 · <a href="README.en.md">English</a>
</p>

<p align="center">
  <a href="#功能">功能</a> ·
  <a href="#下载">下载</a> ·
  <a href="#构建">构建</a> ·
  <a href="#打包">打包</a> ·
  <a href="#许可">许可</a>
</p>

---

## 功能

- [触发器](docs/features/trigger.md)：鼠标或键盘连击、长按与固定坐标输入。
- [模拟粘贴](docs/features/paste.md)：在不支持粘贴的输入框中逐字输入文本。
- [事件剧本](docs/features/event-script.md)：组合文字、按键、鼠标与循环操作。

使用方法与完整命令说明见[中文用户手册](docs/index.md)。

## 下载

Flori Input 提供 Windows x64 安装版和便携版，可从 [GitHub Releases](https://github.com/flowersauce/Flori-Input/releases) 下载。

也可通过 winget 安装或更新：

```powershell
winget install --id Flowersauce.FSClicker --exact --source winget
winget upgrade --id Flowersauce.FSClicker --exact --source winget
```

包标识沿用 `Flowersauce.FSClicker`，软件名称为 Flori Input。1.4.0 起的 winget 清单声明了
`Microsoft.VCRedist.2015+.x64` 依赖，由 winget 处理运行库安装。winget 收录更新可能晚于 GitHub Release；最新版本以 Releases 为准。

**直接下载安装器或便携包时**，请先安装最新的 [Microsoft Visual C++ v14 Redistributable（x64）](https://aka.ms/vc14/vc_redist.x64.exe)。
这两个下载包不包含 VC++ 运行库；已安装最新 x64 运行库的用户无需重复安装。

安装步骤、更新方式与配置保存位置见[安装与更新](docs/getting-started/installation.md)。

## 构建

项目使用 C++23、Slint 1.17.1 和 Win32；构建需要 Windows、CMake 3.30+、Visual Studio C++ x64 生成工具、Ninja、Slint C++ SDK 和
vcpkg（Glaze 8.3.0）。

在 VS x64 开发环境中，将 `VCPKG_ROOT` 和 `SLINT_ROOT` 指向本机 vcpkg 与 Slint SDK 根目录，先安装 manifest 依赖：

```powershell
& "$env:VCPKG_ROOT/vcpkg.exe" install --triplet x64-windows --x-install-root="$PWD/vcpkg_installed"
```

配置示例：

```powershell
cmake -S . -B build/Release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  "-DVCPKG_INSTALLED_DIR=$PWD/vcpkg_installed" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DVCPKG_MANIFEST_INSTALL=OFF `
  -DCMAKE_PREFIX_PATH="$env:SLINT_ROOT"
```

构建：

```powershell
cmake --build build/Release
```

## 打包

应用图标统一从 SVG 生成。修改 Logo 后，先按[应用图标生成](tools/assets/README.md)更新 ICO，再重新编译；通用 PNG 按需单独输出。

从已有的 Release 构建一次生成便携 ZIP、普通 MSI、未签名商店 MSIX 和统一校验文件 `Flori-Input-v<版本>-SHA256SUMS.txt`，直接输出到 `output`。
准备项目 Python 环境、[WiX 7.0.0 及扩展](tools/packaging/windows/README.md)和[Windows SDK](tools/packaging/msix/README.md)后执行：

```powershell
uv run --locked python tools/release/package_app.py
```

默认读取 `build/Release`，不会自动编译。三种包全部制作成功后才覆盖本地同名产物，并清理输出目录中的旧版发布包及校验文件；其它文件和子目录保留。`--build-dir`、`--output-dir`、`--wix`、`--sdk-bin` 可覆盖默认路径。

统一校验文件每行包含一个发布包的 SHA256 和文件名。签名后可运行 `uv run --locked python tools/release/generate_checksums.py` 更新校验文件，不重新打包；该入口也会删除对应的旧单包 `.sha256` 文件。

便携 ZIP 包含 `portable.flag`，配置和剧本保存在程序旁。未带该标记的开发构建使用
`%LOCALAPPDATA%\Flori-Input`，诊断日志位于数据目录的 `logs` 子目录。

开发分支已接入原生 MSI，普通下载和 winget 社区源共用该安装包。
[Windows MSI 制作与验证](tools/packaging/windows/README.md)中包含 WiX 7.0.0、UI/Util 扩展和许可准备步骤。

`uv run --locked python tools/release/release.py` 生成三种包和基于实际 MSI 的 winget 清单，不编译、不上传；清单位于 `output/winget-manifests`。
MSI 程序目录为 `%LOCALAPPDATA%\Programs\Flori-Input`，普通安装与 winget 在安装、升级和修复时均不提供自定义程序目录。
升级和修复沿用已登记的目录，不支持借此修改位置；需要自行安排文件位置时可使用便携版。
**新 MSI 真正卸载时默认删除 `%LOCALAPPDATA%\Flori-Input` 中的全部配置、剧本、日志和预览数据；
需要保留的内容请提前复制到该目录之外。** 升级和修复保留数据。

商店 MSIX 已接入正式包身份、Desktop VCLibs 依赖和图标生成，默认与 MSI、ZIP 一起制作。
本地侧载使用 `output/testing` 中单独签名的副本，具体步骤见[商店 MSIX 制作与验证](tools/packaging/msix/README.md)。
商店审核与上架尚未完成，当前开发产物尚未正式发布。

发布前应验证目标 Windows 版本上的运行与安装。配置和剧本的保存位置见[安装与更新](docs/getting-started/installation.md)。

## 许可

Copyright © 2024 Flowersauce

Flori Input 使用 [MIT 许可证](LICENSE)；第三方组件见[许可说明](docs/legal/third-party.md)。

<a href="https://slint.dev"><img alt="#MadeWithSlint" src="https://raw.githubusercontent.com/slint-ui/slint/master/logo/MadeWithSlint-logo-whitebg.png" height="24"></a>
