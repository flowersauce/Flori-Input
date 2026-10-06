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

The package identifier remains `Flowersauce.FSClicker`. The new MSI and legacy EXE have different installation
identities. Back up and transfer data when migrating; keeping the winget identifier does not guarantee automatic migration.
Starting with 1.4.0, its manifest declares the `Microsoft.VCRedist.2015+.x64` runtime dependency, which winget handles
during installation.
New releases may take time to appear in the winget source. If the desired version is not listed yet, use the installer
from GitHub Releases.

Flori Input uses a per-user installation. Installing or updating the system-wide VC++ runtime may prompt for administrator approval (UAC). If winget finds the required runtime already installed, that dependency does not need to be installed again.

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

The upcoming installer uses `Flori-Input-v<version>-windows-x64-setup.msi`; the MSI instructions below apply to
that development version, which has not been released. Existing EXE releases use the legacy layout.
After installing the runtime, follow the MSI prompts. The program directory is `%LOCALAPPDATA%\Programs\Flori-Input`,
including for silent installation. Installation, upgrade and repair offer no custom directory option. Upgrades and
repairs retain the registered program directory without allowing a location change; the data directory stays separate.
Use the portable ZIP if you need another program location.
Stop input tasks and close the app before installing, updating, repairing or uninstalling.

## Portable package

After installing the runtime, download `Flori-Input-v<version>-windows-x64-portable.zip`, extract it to a writable
directory, then run
`Flori-Input.exe`. Do not run the app directly from the ZIP. New portable packages use `portable.flag` next to the
executable to enable portable mode; keep this file. The portable package stores settings in
`config/config.jsonc` next to the app and event scripts in the neighboring `scripts` directory. You can move both
directories with the portable app.

## Where settings and scripts are stored

The new MSI stores settings and scripts in a separate directory for the current user:

```text
%LOCALAPPDATA%\Flori-Input\
├─ config\config.jsonc
├─ scripts\
├─ logs\
└─ preview\
```

Direct MSI downloads and the winget community source share this data directory. Click **Open scripts folder** on
the **Script** page; its parent is the data root. Upgrades and repairs preserve data. Store MSIX uses separate
package storage managed by Windows: configuration and scripts in `LocalState`, and logs in `LocalCache`.
Store certification and publication are still pending; the Store channel is not available as a released option yet.

## Uninstalling the new MSI

Uninstall through Windows Settings or run `winget uninstall --id Flowersauce.FSClicker --exact --source winget`.
This removes the MSI's program files, shortcuts and registration, plus all configuration, scripts, logs and preview
data under `%LOCALAPPDATA%\Flori-Input`. No cleanup parameter or option to retain data is provided.
**Copy anything you want to keep outside that data directory before uninstalling.** Exported files, portable
directories and Store data are outside the MSI cleanup scope. Removing the old MSI during an upgrade skips data cleanup.

## Migrating from the legacy EXE

**The new MSI cannot upgrade the legacy 1.4.0 Velopack EXE installation in place. Uninstall the old version manually first.**

1. Close the old app. If you want to keep settings and scripts, back up `config` and `scripts` from `%LOCALAPPDATA%\Flowersauce.FSClicker` to another location.
2. Uninstall **Flori Input 1.4.0** manually through Windows Settings.
3. Press `Win + R`, enter `%LOCALAPPDATA%`, and delete the remaining `Flowersauce.FSClicker` folder, if present.
4. Install the new MSI.

To restore your backup, copy `config` and `scripts` into `%LOCALAPPDATA%\Flori-Input` while the app is closed.
For a custom legacy location, use the old app's **Open scripts folder** to find it before backing up and cleaning that directory.

