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

包标识继续使用 `Flowersauce.FSClicker`，以保留从 FSClicker 升级的路径，软件名称为 Flori Input。
1.4.0 起的清单声明了 `Microsoft.VCRedist.2015+.x64` 运行库依赖，安装时由 winget 处理。
新版本收录到 winget 可能晚于 GitHub Release；若尚未查到目标版本，可从 Releases 下载安装版。

## 直接下载所需的运行库

使用 Releases 中的安装器或便携 ZIP 前，请先安装最新的
[Microsoft Visual C++ v14 Redistributable（x64）](https://aka.ms/vc14/vc_redist.x64.exe)。
这两个包不包含、也不会自动安装运行库。已有最新 x64 运行库时无需重复安装；仅安装 x86 版本不能满足要求。
详细说明见[微软运行库下载文档](https://learn.microsoft.com/zh-cn/cpp/windows/latest-supported-vc-redist)。

若启动时提示缺少 `MSVCP140.dll`、`MSVCP140_ATOMIC_WAIT.dll` 或 `VCRUNTIME140*.dll`，请安装或修复 x64 运行库后重试。

## 安装版

安装运行库后，下载 `Flori-Input-v<版本>-windows-x64-setup.exe` 并按提示安装。升级前请停止正在运行的输入任务并关闭程序。

## 便携版

安装运行库后，下载 `Flori-Input-v<版本>-windows-x64-portable.zip`，解压到可写目录后运行 `Flori-Input.exe`
。不要直接从压缩包内启动。便携版的配置保存在程序目录的
`config/config.jsonc`，事件剧本放在同级的 `scripts` 目录；移动便携版时可一起移动这两个目录。

## 配置与剧本保存位置

安装版将配置和剧本保存在安装根目录，与 `current` 版本目录平级：

```text
<安装根目录>\
├─ current\Flori-Input.exe
├─ config\config.jsonc
└─ scripts\
```

安装位置可能因安装方式而异。需要找到实际目录时，可先在“事件剧本”页点击“打开剧本目录”；其上一级就是安装根目录，
`config\config.jsonc` 位于同一级。更新安装版时，`config` 和 `scripts` 不在 `current` 目录内，不会随版本目录一同替换。请保留这两个目录以备份设置和剧本。

