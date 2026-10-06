# 安装与更新

Flori Input 面向 Windows。安装包和便携包可从[项目 Releases](https://github.com/flowersauce/Flori-Input/releases)
获取。下载前请核对发布者和文件名，不要从不明镜像获取安装包。

## winget 安装与更新

安装：

```powershell
winget install --id Flowersauce.FSClicker --exact --source winget
```

更新前先停止输入任务并关闭软件，然后执行：

```powershell
winget upgrade --id Flowersauce.FSClicker --exact --source winget
```

包标识继续使用 `Flowersauce.FSClicker`，软件名称为 Flori Input。新 MSI 与旧 EXE 使用不同安装身份，
首次迁移需按下方步骤备份与转移数据，不能把保留 winget 标识视为自动迁移保证。
1.4.0 起的清单声明了 `Microsoft.VCRedist.2015+.x64` 运行库依赖，安装时由 winget 处理。
新版本收录到 winget 可能晚于 GitHub Release；若尚未查到目标版本，可从 Releases 下载安装版。

Flori Input 本体按当前用户安装；VC++ 运行库属于系统级组件，安装或更新运行库时可能弹出管理员授权（UAC）。如果 winget 已识别到满足要求的运行库，就无需重复安装该依赖。

## 直接下载所需的运行库

使用 Releases 中的安装器或便携 ZIP 前，请先安装最新的
[Microsoft Visual C++ v14 Redistributable（x64）](https://aka.ms/vc14/vc_redist.x64.exe)。
这两个包不包含、也不会自动安装运行库。已有最新 x64 运行库时无需重复安装；仅安装 x86 版本不能满足要求。
详细说明见[微软运行库下载文档](https://learn.microsoft.com/zh-cn/cpp/windows/latest-supported-vc-redist)。

若启动时提示缺少 `MSVCP140.dll`、`MSVCP140_ATOMIC_WAIT.dll` 或 `VCRUNTIME140*.dll`，请安装或修复 x64 运行库后重试。

## 安装版

正在准备的新安装版使用 `Flori-Input-<版本>-windows-x64-setup.msi`，以下 MSI 说明适用于该新版本，
尚未正式发布；既有 Release 的 EXE 安装器仍采用旧布局。安装运行库后按 MSI 提示安装，程序目录为
`%LOCALAPPDATA%\Programs\Flori-Input`，静默安装使用相同目录。安装、升级和修复均不提供自定义目录入口。
升级和修复沿用已登记程序目录，不支持借此修改位置，数据目录保持独立。需要自行安排程序位置时使用便携 ZIP。
安装、升级、修复和卸载前请停止输入任务并关闭程序。

## 便携版

安装运行库后，下载 `Flori-Input-<版本>-windows-x64-portable.zip`，解压到可写目录后运行 `Flori-Input.exe`
。不要直接从压缩包内启动。新便携包通过程序旁的 `portable.flag` 识别便携模式，请保留该文件；配置保存在程序目录的
`config/config.jsonc`，事件剧本放在同级的 `scripts` 目录；移动便携版时可一起移动这两个目录。

## 配置与剧本保存位置

新 MSI 将配置和剧本保存在当前用户的独立数据目录：

```text
%LOCALAPPDATA%\Flori-Input\
├─ config\config.jsonc
├─ scripts\
├─ logs\
└─ preview\
```

普通下载安装和 winget 社区源使用同一数据目录。在“事件剧本”页点击“打开剧本目录”，其上一级就是数据根目录。
更新和修复保留用户数据。商店 MSIX 使用系统管理的包存储，与普通 MSI 分开；配置和剧本使用包的
`LocalState`，日志使用 `LocalCache`。商店审核与上架尚未完成，商店渠道尚未正式开放。

## 卸载新 MSI

从系统设置卸载，或执行 `winget uninstall --id Flowersauce.FSClicker --exact --source winget`，
默认删除本 MSI 的程序文件、快捷方式、安装登记，以及 `%LOCALAPPDATA%\Flori-Input` 下的全部配置、
剧本、日志和预览数据，没有额外清理参数或保留数据选项。**需要保留的内容请在卸载前复制到该数据目录之外。**
用户导出的文件、便携目录和商店数据不会由普通 MSI 清理；升级过程中旧 MSI 的移除不触发数据清理。

## 旧 EXE 安装版迁移

**新 MSI 与旧 1.4.0（Velopack EXE）安装方式不兼容，不能直接覆盖升级，请先手动卸载旧版。**

1. 退出旧版。如需保留设置和剧本，将 `%LOCALAPPDATA%\Flowersauce.FSClicker` 中的 `config` 和 `scripts` 备份到其他位置。
2. 在 Windows 系统设置中手动卸载 **Flori Input 1.4.0**。
3. 按 `Win + R`，输入 `%LOCALAPPDATA%`，删除残留的 `Flowersauce.FSClicker` 文件夹；不存在则跳过。
4. 安装新 MSI。

需要恢复备份时，在软件关闭状态下，将 `config` 和 `scripts` 复制到 `%LOCALAPPDATA%\Flori-Input`。
旧版若安装在其他位置，可通过旧版“打开剧本目录”确认位置，备份后清理对应的旧安装目录。

