# 商店 MSIX 制作与本地验证

此入口从现有 Release 构建制作未签名的 x64 MSIX，不编译应用、下载安装工具、创建或信任证书、安装应用或提交商店。商店提交和审核由开发者另行完成。

## 包身份与运行方式

| 字段 | 值 |
| --- | --- |
| Identity Name | `Flowersauce.FloriInput` |
| Identity Publisher | `CN=4276B3BE-2A10-4D07-BF6B-2419400FDCF6` |
| PublisherDisplayName | `Flowersauce` |
| PFN | `Flowersauce.FloriInput_fw24f0by96da4` |
| Application Id | `FloriInput` |
| Store ID | `9NL38SKR3TKT` |

`AppxManifest.xml.in` 集中维护正式身份、最低系统和运行库依赖。项目版本 `1.5.0` 对应 MSIX `1.5.0.0`，商店保留的第四段始终为 0。

目标为 Windows 10 2004（19041）及以上桌面系统，x64；`MaxVersionTested` 对应当前 SDK 的 26100 平台声明，不代替真实兼容性验收。应用使用 `packagedClassicApp` / `mediumIL` 和 `runFullTrust`，保持普通桌面权限，不增加自动提权或服务。包声明中文和英文，界面仍使用应用已有的语言资源。

商店版程序位置由 Windows 管理，配置和剧本使用包的 `LocalFolder`，日志使用 `LocalCacheFolder`；`--preview` 继续使用各自的 `preview` 子目录。没有 `portable.flag`，不安装配置或剧本，不与普通 MSI 共享数据。完整卸载按系统默认行为清理包私有数据；更新保留数据，用户复制或导出的外部文件保留。

## 工具和运行库

需要已有 Windows SDK 的 `makeappx.exe`、`makepri.exe`，以及项目 `.venv` 中的 resvg 开发依赖（`uv sync --locked`）。本地签名另需同 SDK 的 `signtool.exe`。默认查找本机已安装的最新 SDK x64 工具目录，亦可通过 `--sdk-bin` 显式指定，例如：

```text
C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64
```

MSIX 声明 `Microsoft.VCLibs.140.00.UWPDesktop`，最低版本 `14.0.33728.0`。商店分发时由商店处理框架依赖；本地侧载可使用 Visual Studio 已有的 Retail x64 包：

```text
C:\Program Files (x86)\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs.Desktop\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.Desktop.appx
```

MSIX 使用 Desktop VCLibs 框架依赖，不复制普通 VCRedist DLL；系统中已安装的普通 VCRedist 不能代替这个包依赖。

图标由 `tools/assets/generate_icons.py` 直接从统一源 `resources/icons/Flori-Input.svg` 渲染清单 PNG 和任务栏尺寸变体，再由 MakePri 生成 `resources.pri`。所有 MSIX 素材只存在于打包暂存目录。应用 ICO 和可选单张通用 PNG 的生成入口见 [应用图标生成](../../assets/README.md)。

## 制作未签名包

在项目根目录执行，复用已有 Release 构建：

```powershell
uv run --locked python tools/release/package_app.py
```

默认将 `Flori-Input-<版本>-windows-x64-store.msix` 输出到 `output/store`；便携 ZIP、普通 MSI 和统一的 `Flori-Input-<版本>-SHA256SUMS.txt` 放在 `output/public`。GitHub Release 只上传 `public` 中的三个文件；商店 MSIX 单独提交，不另生成校验文件，也不加入公开校验清单。需要已安装 WiX 7.0.0 及 UI/Util 扩展。三种包共用相同应用构建；全部制作成功后覆盖同名文件，并按发布命名规则清理输出根目录及 `public`、`store` 中的旧产物。MSIX 不含便携标记，不自动签名；临时素材随本次打包清理。`--output-dir`、`--wix`、`--sdk-bin` 可覆盖默认路径；自定义输出根目录同样使用 `public`、`store` 子目录。

正式版本统一且各渠道验证通过后，可生成三种产物及 MSI 的 winget 清单：

```powershell
uv run --locked python tools/release/release.py
```

此命令不上传产物；社区 winget 清单仍指向 MSI，商店 MSIX 不写入该社区清单。商店尚未上架，不能据 Store ID 宣称安装命令已经可用。

## 本地签名与安装

未签名商店包可以用于后续商店提交；本地安装需要单独签名。以下步骤由开发者显式执行，使用同一 Windows 账户。测试包与正式商店使用相同 PFN，已有该包的测试数据需要在完整卸载前备份。

先在项目根目录的 PowerShell 中复制测试副本；当前版本示例为 1.5.0：

```powershell
$storeMsix = (Resolve-Path '.\output\store\Flori-Input-1.5.0-windows-x64-store.msix').Path
$testDirectory = Join-Path $PWD 'output\testing'
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
$testMsix = Join-Path $testDirectory 'Flori-Input-1.5.0-windows-x64-development.msix'
Copy-Item -LiteralPath $storeMsix -Destination $testMsix
$sdkBin = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64'
```

首次本地侧载时，在同一账户的管理员 PowerShell 中设置以上变量，再创建开发证书、仅导出公钥并信任；私钥保存在用户证书库，不导出 PFX 或写入仓库。已有开发证书时跳过创建和信任步骤，按下方“复用证书与升级验收”签名：

```powershell
$publisher = 'CN=4276B3BE-2A10-4D07-BF6B-2419400FDCF6'
$devCertificate = New-SelfSignedCertificate -Type Custom -KeyUsage DigitalSignature `
    -Subject $publisher -CertStoreLocation 'Cert:\CurrentUser\My' `
    -KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256 -KeyExportPolicy NonExportable `
    -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}') `
    -FriendlyName 'Flori Input local MSIX validation'
$certificateFile = Join-Path (Split-Path -Parent $testMsix) 'FloriInput-development.cer'
Export-Certificate -Cert $devCertificate -FilePath $certificateFile | Out-Null
Import-Certificate -FilePath $certificateFile -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null
& (Join-Path $sdkBin 'signtool.exe') sign /fd SHA256 /s My /sha1 $devCertificate.Thumbprint $testMsix
if ($LASTEXITCODE -ne 0) { throw 'MSIX 签名失败，停止安装。' }
& (Join-Path $sdkBin 'signtool.exe') verify /pa $testMsix
if ($LASTEXITCODE -ne 0) { throw 'MSIX 签名校验失败，停止安装。' }
```

记录 `$devCertificate.Thumbprint`，后续可复用它签名更高版本的测试副本，不必每次新建证书。签名只修改 `output/testing` 中的 `development.msix`；商店包保持原样，公开校验清单仅包含 ZIP 和 MSI。重新打包不清理 `testing`；本地测试结束后由开发者清理需要移除的副本和日志。

回到普通权限 PowerShell，在项目根目录重新设置 `$testMsix` 后安装。先检查当前用户是否已有满足要求的 x64 Desktop VCLibs；已有时直接复用，仅缺少时通过 `-DependencyPath` 提供本机 Retail x64 框架：

```powershell
$testMsix = (Resolve-Path '.\output\testing\Flori-Input-1.5.0-windows-x64-development.msix').Path
$installedVclibs = Get-AppxPackage -Name 'Microsoft.VCLibs.140.00.UWPDesktop' -ErrorAction Stop |
    Where-Object {
        $_.Architecture -eq 'X64' -and
        [version]$_.Version -ge [version]'14.0.33728.0' -and
        $_.Publisher -eq 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US' -and
        $_.Status -eq 'Ok'
    }
if ($installedVclibs) {
    Add-AppxPackage -Path $testMsix -ErrorAction Stop
} else {
    $desktopVclibs = 'C:\Program Files (x86)\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs.Desktop\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.Desktop.appx'
    Add-AppxPackage -Path $testMsix -DependencyPath $desktopVclibs -ErrorAction Stop
}
$package = Get-AppxPackage -Name 'Flowersauce.FloriInput' -ErrorAction Stop
if ($null -eq $package) {
    throw '未发现已安装的 Flori Input，请先处理上面的部署错误。'
}
if ($package.PackageFamilyName -ne 'Flowersauce.FloriInput_fw24f0by96da4') {
    throw '安装包身份不符合预期。'
}
$package | Select-Object Name, Version, PackageFamilyName, InstallLocation
```

若出现 `0x80073D02`，先读取错误中给出的 `Get-AppPackageLog -ActivityID ...`，确认被占用的包。已有满足要求的 VCLibs 时，省略 `-DependencyPath`，避免再次部署共享框架；缺少依赖时，关闭使用该框架的应用后重试。

## 复用证书与升级验收

升级验证使用实际旧版安装，并先备份需要保留的设置和剧本。退出两渠道应用，完成更高版本的 Release 构建与统一打包。下方命令以 1.5.0 为例，会复制并签名 `output/testing` 中的开发副本后升级；实际使用时替换版本号。两渠道显示名称均为 Flori Input，测试时从各自安装目录启动。

复用已成功签名的开发证书，以精确 Thumbprint 选择。可查询候选证书的公钥元数据；已有成功签名副本时，也可用 `(Get-AuthenticodeSignature -LiteralPath '副本路径').SignerCertificate.Thumbprint` 确认实际签名者，不按同名候选数组的第一项选择：

```powershell
Get-ChildItem 'Cert:\CurrentUser\My' |
    Where-Object {
        $_.Subject -eq 'CN=4276B3BE-2A10-4D07-BF6B-2419400FDCF6' -and
        $_.FriendlyName -eq 'Flori Input local MSIX validation'
    } | Select-Object Subject, Thumbprint, NotAfter, HasPrivateKey
```

在普通权限 PowerShell 中填写已记录的精确 Thumbprint，再将以下脚本块整段一次执行；任何一步报错都会中止本次调用，避免逐条输入时检查失败后仍继续签名或安装。此例复用已确认存在且满足要求的 VCLibs：

```powershell
& {
    $devThumbprint = '<已成功签名的开发证书 Thumbprint>'
    $devCertificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$devThumbprint" -ErrorAction Stop
    if ($devCertificate.Subject -ne 'CN=4276B3BE-2A10-4D07-BF6B-2419400FDCF6' -or
        -not $devCertificate.HasPrivateKey -or $devCertificate.NotAfter -le (Get-Date)) {
        throw '开发证书 Publisher 不匹配、缺少私钥或已过期，停止升级。'
    }
    $sdkBin = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64'
    $storeMsix = (Resolve-Path '.\output\store\Flori-Input-1.5.0-windows-x64-store.msix' -ErrorAction Stop).Path
    $testDirectory = Join-Path $PWD 'output\testing'
    New-Item -ItemType Directory -Path $testDirectory -Force -ErrorAction Stop | Out-Null
    $testMsix = Join-Path $testDirectory 'Flori-Input-1.5.0-windows-x64-development.msix'
    Copy-Item -LiteralPath $storeMsix -Destination $testMsix -ErrorAction Stop
    & (Join-Path $sdkBin 'signtool.exe') sign /fd SHA256 /s My /sha1 $devCertificate.Thumbprint $testMsix
    if ($LASTEXITCODE -ne 0) { throw 'MSIX 签名失败，停止升级。' }
    & (Join-Path $sdkBin 'signtool.exe') verify /pa $testMsix
    if ($LASTEXITCODE -ne 0) { throw 'MSIX 签名校验失败，停止升级。' }
    Add-AppxPackage -Path $testMsix -ErrorAction Stop
    Get-AppxPackage -Name 'Flowersauce.FloriInput' -ErrorAction Stop |
        Select-Object Name, Version, PackageFamilyName, InstallLocation
}
```

升级后核对 MSIX 版本，PFN 应保持相同；包安装位置随版本变化，由系统管理，启动前重新查询 InstallLocation。再核对原配置和剧本，并确认跨渠道互斥和数据隔离仍有效。MSI 升级使用对应的新版 `setup.msi`，旧产品登记应被替换，新产品保留原安装路径和用户数据。

## 验收

退出普通 MSI / 开发运行实例，然后从开始菜单启动商店测试版。确认：

1. 正常启动、设置重启持久化；配置和剧本位于包的 `LocalState`，日志位于 `LocalCache`；普通 MSI 数据不被修改。
2. 热键、Raw Input、鼠标钩子、SendInput、停止和退出释放行为正常；剧本编辑与打开目录可用。
3. 预览数据隔离；MSI / MSIX 的跨渠道互斥有效，关闭后可以正常重启。
4. 使用实际更高版本的签名副本验证更新保留配置和剧本；重复制作同一版本不算更新验证。
5. 若系统提供“移动”入口，在允许的 NTFS 目标卷验证移动后启动、保存和更新，不手动搬动 WindowsApps。
6. 发布包自动附带 `LICENSE` 和 `THIRD-PARTY-NOTICES.txt`；原文来源见[许可目录](../../../resources/licenses/README.md)。Windows App Certification Kit 用于兼容性检查；商店截图、描述、年龄分级、受限能力说明及所需隐私政策由开发者在 Partner Center 提交审核前补齐。

从“事件剧本”页打开目录可确认实际数据根目录；本机常见位置为：

```text
%LOCALAPPDATA%\Packages\Flowersauce.FloriInput_fw24f0by96da4\LocalState\config\config.jsonc
%LOCALAPPDATA%\Packages\Flowersauce.FloriInput_fw24f0by96da4\LocalState\scripts\
%LOCALAPPDATA%\Packages\Flowersauce.FloriInput_fw24f0by96da4\LocalCache\logs\
```

诊断日志仅在传入 `--diagnose-input` 时生成。先退出已运行实例，再在普通权限 PowerShell 中依次启动正常模式和预览模式；每次检查界面后退出应用，命令才继续。预览中修改设置或剧本后，正式配置的哈希应不变：

```powershell
$package = Get-AppxPackage -Name 'Flowersauce.FloriInput' -ErrorAction Stop
if ($null -eq $package) { throw '请先安装 MSIX 测试版。' }
$msixExe = Join-Path $package.InstallLocation 'Flori-Input.exe'
$packageData = Join-Path $env:LOCALAPPDATA 'Packages\Flowersauce.FloriInput_fw24f0by96da4'
Start-Process -FilePath $msixExe -ArgumentList '--diagnose-input' -Wait
Test-Path (Join-Path $packageData 'LocalCache\logs\slint-input-trace.txt')

$normalConfig = Join-Path $packageData 'LocalState\config\config.jsonc'
$configHashBefore = (Get-FileHash -LiteralPath $normalConfig).Hash
Start-Process -FilePath $msixExe -ArgumentList '--preview', '--diagnose-input' -Wait
Test-Path (Join-Path $packageData 'LocalState\preview\config\config.jsonc')
Test-Path (Join-Path $packageData 'LocalCache\preview\logs\slint-input-trace.txt')
(Get-FileHash -LiteralPath $normalConfig).Hash -eq $configHashBefore
```

上述四项检查均应返回 `True`。正常模式的跨渠道互斥另行验证：保持 MSI 运行再启动 MSIX，以及保持 MSIX 运行再启动 MSI，后启动实例应退出且不出现第二个主窗口；退出原实例后，另一个渠道应能正常启动。预览模式可以独立运行。

退出测试版并备份要保留的包数据后，从系统设置卸载，或在普通权限 PowerShell 执行：

```powershell
Get-AppxPackage -Name 'Flowersauce.FloriInput' | Remove-AppxPackage
Get-AppxPackage -Name 'Flowersauce.FloriInput'
```

完整卸载应移除当前用户包登记和私有数据，普通 MSI 数据及外部导出文件保持完整。不要使用保留数据的卸载参数。

测试结束且不再使用该开发证书时，在同一账户的管理员 PowerShell 中删除本次记录的两处证书；仅使用精确 Thumbprint，不按发布者通配删除：

```powershell
$devThumbprint = '<本次记录的 Thumbprint>'
Remove-Item -LiteralPath "Cert:\LocalMachine\TrustedPeople\$devThumbprint"
Remove-Item -LiteralPath "Cert:\CurrentUser\My\$devThumbprint"
```

## 依据

- [手工生成 MSIX 组件](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion)
- [Desktop Bridge 的 C++ 框架依赖](https://learn.microsoft.com/en-us/troubleshoot/developer/visualstudio/cpp/libraries/c-runtime-packages-desktop-bridge)
- [MakePri 参数](https://learn.microsoft.com/en-us/windows/uwp/app-resources/makepri-exe-command-options)
- [MakeAppx 参数](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool)
- [商店包版本和签名要求](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements)
- [本地签名和证书信任](https://learn.microsoft.com/en-us/windows/msix/package/sign-msix-package-guide)
- [Add-AppxPackage 与已安装依赖的处理](https://learn.microsoft.com/en-us/powershell/module/appx/add-appxpackage)
