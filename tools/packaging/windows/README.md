# Windows MSI 制作与验证

普通下载安装版和 winget 社区源使用同一份按用户安装的 x64 MSI。商店 MSIX 的制作入口已接入，见[MSIX 制作与验证](../msix/README.md)；当前 MSI 不替换旧 Velopack 的产品登记。

## 工具准备

需要 .NET 8 运行时、.NET SDK，以及固定版本 WiX 7.0.0 和官方 UI、Util 扩展。以下命令由开发者执行，WiX 使用 PATH 中的全局工具，项目扩展放在 `.wix/extensions`；已安装者无需重复安装：

```powershell
dotnet tool install --global wix --version 7.0.0
```

使用前阅读 [WiX 7 OSMF EULA](https://github.com/wixtoolset/wix/blob/v7.0.0/OSMFEULA.txt)，根据自身情况确认维护费是否适用，并自行接受条款：

```powershell
wix eula accept wix7
```

接受条款后，再准备扩展：

```powershell
wix extension add WixToolset.UI.wixext/7.0.0
wix extension add WixToolset.Util.wixext/7.0.0
```

扩展命令从项目根目录执行。打包默认使用 `wix`；若使用其它位置的同版本工具，可通过 `--wix` 指定实际路径。工具不放在发布产物目录中。

打包脚本不会下载工具、安装运行库或替开发者接受条款。直接下载安装 MSI 和 ZIP 的用户需要先安装 [VC++ x64 运行库](https://aka.ms/vc14/vc_redist.x64.exe)；winget 清单继续声明该依赖。

## 生成产物

先完成当前版本的 Release 构建，并按[MSIX 说明](../msix/README.md)准备项目 Python 环境和 SDK，再执行：

```powershell
uv run --locked python tools/release/package_app.py
```

以上命令使用已安装的全局 WiX，将 `Flori-Input-<版本>-windows-x64-setup.msi`、便携 ZIP 和统一的 `Flori-Input-<版本>-SHA256SUMS.txt` 放在 `output/public`，未签名商店 MSIX 放在 `output/store`。校验文件仅包含 ZIP 和 MSI 的 SHA256 与文件名。全部制作成功后覆盖同名文件，并按发布命名规则清理输出根目录及 `public`、`store` 中的旧产物，包括旧 `Flori-Input-v…` 命名产物及单包 `.sha256` 文件；其它文件和子目录保留。版本来自 Release 构建缓存，并核对当前 `CMakeLists.txt`；MSI 只接受三段数字版本。`--build-dir`、`--output-dir`、`--wix` 和 `--sdk-bin` 可覆盖默认路径；自定义输出根目录同样使用 `public`、`store` 子目录。Python 脚本不会隐式编译、安装或提交发布。

准备正式发布包与 winget 清单：

```powershell
uv run --locked python tools/release/release.py
```

清单默认从 `output/public` 中的实际 MSI 读取 ProductCode、UpgradeCode、ProductVersion，并计算文件 SHA256；同目录存在统一校验文件时，按 MSI 文件名查找并核对，缺少该项或哈希不符会报错。保留社区包标识 `Flowersauce.FSClicker`；产品 GUID 与该标识分别维护。winget 清单位于 `output/winget-manifests`，不上传文件。GitHub Release 只上传 `output/public` 中的 ZIP、MSI 和校验文件；`output/store` 中的 MSIX 单独用于商店提交，不生成或加入校验清单。

三种包共用程序暂存目录，均附带项目 `LICENSE` 和 `THIRD-PARTY-NOTICES.txt`。第三方声明由仓库内的[许可原文](../../../resources/licenses/README.md)合并生成，涵盖 Slint 及 SDK 依赖、Glaze、内置字体和 Slint 标识；打包无需访问网络。

如果对 MSI 签名，应先单独打包、签名，再更新校验文件和生成清单，不重新执行会覆盖已签名文件的完整发布入口：

```powershell
$msiFiles = @(Get-ChildItem .\output\public\*-setup.msi)
if ($msiFiles.Count -ne 1) { throw '请明确选择要签名的 MSI。' }
$msiPath = $msiFiles[0].FullName
# 在此使用自己的证书完成签名，然后重新计算最终文件的校验值。
uv run --locked python tools/release/generate_checksums.py
uv run --locked python tools/release/generate_winget_manifest.py --installer "$msiPath"
```

不要覆盖已公开发布的同版本附件。升级验证使用实际旧版安装和更高版本的 MSI。

## 安装行为

- 程序目录为 `%LOCALAPPDATA%\Programs\Flori-Input`。安装、升级和修复均不提供自定义目录入口，界面使用 `WixUI_Minimal`；静默安装同样在成本计算前设定固定路径，外部 `INSTALLFOLDER` 参数不改变新安装位置。winget 清单不再提供应用的 InstallLocation 映射；其 MSI 默认 `TARGETDIR` 参数也不改变应用的固定绝对路径。
- HKCU 中记录实际目录；升级、修复和卸载沿用该目录，不允许通过维护操作选择新位置。读取已登记目录也用于正确维护此前安装到其他位置的开发 MSI，不是自定义目录入口。需要自行安排文件位置时使用便携 ZIP。
- 开始菜单提供 Flori Input 入口，系统设置提供修复/卸载登记；不安装 `portable.flag`、用户配置或剧本。
- 固定 UpgradeCode；同一版本固定 ProductCode，新版本更换 ProductCode，使用 Major Upgrade 并阻止降级。正式版本不通过重新制作同版本 MSI 更新；修复使用已安装的原始 MSI。
- 安装、修复和卸载前要求退出运行中的 `Flori-Input.exe`，不发送关闭消息或强制终止输入任务。
- 真正卸载默认删除当前用户 `%LOCALAPPDATA%\Flori-Input` 下的全部配置、剧本、日志和预览数据，没有自定义保留数据入口。升级中的旧 MSI 卸载跳过清理；修复和安装失败回滚不会触发清理。
- 只清理本 MSI 的程序资源和普通版私有数据，不清理商店、便携、其他用户或旧 Velopack 目录，也不删除复制/导出到私有目录之外的文件。

## 开发者手动验证

**卸载验证会删除已有普通版数据。先退出应用，将 `%LOCALAPPDATA%\Flori-Input` 中需要保留的内容复制到该目录之外。**

从项目根目录选择要验证的 MSI，并使用标准安装界面：

```powershell
$msiFiles = @(Get-ChildItem .\output\public\*-setup.msi)
if ($msiFiles.Count -ne 1) { throw '请明确指定要验证的 MSI 完整路径。' }
$msiPath = $msiFiles[0].FullName
New-Item -ItemType Directory -Path '.\output\testing' -Force | Out-Null
$installLog = Join-Path $PWD 'output\testing\msi-install.log'
$process = Start-Process msiexec.exe -ArgumentList @('/i', "`"$msiPath`"", '/L*v', "`"$installLog`"") -Wait -PassThru
$process.ExitCode
```

| 路径 | 应确认的结果 |
| --- | --- |
| 首次安装 | 按用户安装；没有目录选择页；程序位于 `%LOCALAPPDATA%\Programs\Flori-Input`；开始菜单与系统登记存在；程序目录无便携标记；配置仍在普通版数据目录 |
| 静默新安装 | `/i "MSI路径" /qn` 使用相同固定目录；在无旧产品登记时，即使传入 `INSTALLFOLDER="其它目录"` 或 `TARGETDIR="其它目录"`，实际程序路径仍为固定目录 |
| 正在运行 | 安装、修复、卸载失败并要求先退出；已有输入任务不会被安装器强制终止 |
| 修复 | 使用原始 MSI 的 `/fa "MSI路径"`；沿用已登记程序目录；配置与剧本不变 |
| 新版升级 | 使用实际更高版本构建的 MSI；沿用程序目录；旧产品登记移除；设置、剧本、日志和预览数据保留 |
| 降级 | 更低版本 MSI 被阻止；既有应用和数据保留 |
| 系统卸载 | 系统设置卸载后，本 MSI 的文件、快捷方式、登记及普通版数据目录均移除；外部备份保留 |
| 静默卸载 | `/x "MSI路径" /qn` 同样默认清理全部普通版数据，不加任何清理参数 |

生成本地 winget 清单并检查格式：

```powershell
python tools/release/generate_winget_manifest.py --installer "$msiPath"
$version = '1.5.0' # 改为所选 MSI 的实际版本。
winget validate --manifest ".\output\winget-manifests\manifests\f\Flowersauce\FSClicker\$version"
```

本地 winget 安装验证需启用本地清单支持，并让清单 URL 指向可访问的同一份 MSI。发布后的 `winget install/upgrade/uninstall --id Flowersauce.FSClicker --exact --source winget` 应遵循同一 MSI 行为；旧 EXE 到新 MSI 的迁移需备份与独立迁移验证，不能据 MSI 升级测试认定其自动兼容。

实现依据：[WiX CLI](https://docs.firegiant.com/wix/tools/wixexe/)、[MajorUpgrade](https://docs.firegiant.com/wix/schema/wxs/majorupgrade/)、[RemoveFolderEx](https://docs.firegiant.com/wix/schema/util/removefolderex/)、[winget 清单格式](https://learn.microsoft.com/en-us/windows/package-manager/package/manifest)。
