# Install and Update

Flori Input runs on Windows. Installer and portable packages are available from
the [project Releases](https://github.com/flowersauce/Flori-Input/releases). Check the publisher and filename before
downloading, and avoid untrusted mirrors.

## winget

Install:

```powershell
winget install --id Flowersauce.FSClicker --exact --source winget
```

Before updating, stop active input tasks and close the app, then run:

```powershell
winget upgrade --id Flowersauce.FSClicker --exact --source winget
```

The package identifier remains `Flowersauce.FSClicker` to preserve the upgrade path from FSClicker.
Starting with 1.4.0, its manifest declares the `Microsoft.VCRedist.2015+.x64` runtime dependency, which winget handles
during installation.
New releases may take time to appear in the winget source. If the desired version is not listed yet, use the installer
from GitHub Releases.

## Runtime for direct downloads

Before using an installer or portable ZIP downloaded from Releases, install the latest
[Microsoft Visual C++ v14 Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe).
These packages do not bundle or automatically install the runtime. An existing up-to-date x64 runtime is sufficient; the
x86 runtime alone is not.
See [Microsoft's runtime download documentation](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
for details.

If startup reports a missing `MSVCP140.dll`, `MSVCP140_ATOMIC_WAIT.dll`, or `VCRUNTIME140*.dll`, install or repair the
x64 runtime and try again.

## Installer

After installing the runtime, download `Flori-Input-v<version>-windows-x64-setup.exe` and follow the installer prompts.
Before updating, stop any
active input task and close the app.

## Portable package

After installing the runtime, download `Flori-Input-v<version>-windows-x64-portable.zip`, extract it to a writable
directory, then run
`Flori-Input.exe`. Do not run the app directly from the ZIP. The portable package stores settings in
`config/config.jsonc` next to the app and event scripts in the neighboring `scripts` directory. You can move both
directories with the portable app.

## Where settings and scripts are stored

The installer stores settings and scripts in the installation root, alongside the `current` version directory:

```text
<installation root>\
├─ current\Flori-Input.exe
├─ config\config.jsonc
└─ scripts\
```

The actual installation location depends on how the app was installed. To find it, click **Open scripts folder** on the
**Script** page. Its parent is the installation root; `config\config.jsonc` is alongside `scripts`. Because `config` and
`scripts` are outside `current`, an installer update does not replace them with the version directory. Keep copies of
both directories when backing up your settings and scripts.

