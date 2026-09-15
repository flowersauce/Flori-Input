# 安装与更新

Flori Input 面向 Windows。安装包和便携包可从[项目 Releases](https://github.com/flowersauce/Flori-Input/releases)
获取。下载前请核对发布者和文件名，不要从不明镜像获取安装包。

## 安装版

下载 `Flori-Input-v<版本>-windows-x64-setup.exe` 并按提示安装。升级前请停止正在运行的输入任务并关闭程序。

## 便携版

下载 `Flori-Input-v<版本>-windows-x64-portable.zip`，解压到可写目录后运行 `Flori-Input.exe`。不要直接从压缩包内启动。便携版的配置保存在程序目录的
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
