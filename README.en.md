<p align="center"><img src="resources/icons/Flori-Input_transparent_dynamic.svg" alt="Flori Input" width="120"></p>

<h1 align="center">Flori Input</h1>

<p align="center">A Windows keyboard and mouse input automation tool supporting repeated input, simulated paste, and event scripts. Evolved from FSClicker and rebuilt with Slint UI.</p>

<p align="center">
  <img alt="Windows" src="https://img.shields.io/badge/Windows-0078D4?style=flat-square&logo=windows&logoColor=white">
  <img alt="C++23" src="https://img.shields.io/badge/C%2B%2B-23-00599C?style=flat-square&logo=cplusplus&logoColor=white">
  <img alt="Slint" src="https://img.shields.io/badge/Slint-2379F4?style=flat-square&logo=Slint&logoColor=white">
  <img alt="License" src="https://img.shields.io/badge/License-MIT-2DA44E?style=flat-square">
</p>

<p align="center">
  <a href="README.md">中文</a> · English
</p>

<p align="center">
  <a href="#features">Features</a> ·
  <a href="#download">Download</a> ·
  <a href="#build">Build</a> ·
  <a href="#packaging">Packaging</a> ·
  <a href="#license">License</a>
</p>

---

## Features

- [Trigger](docs/features/trigger.en.md): Mouse or keyboard repeated actions, long press, and fixed coordinate input.
- [Simulated Paste](docs/features/paste.en.md): Type text character by character in input fields that do not support
  pasting.
- [Event Script](docs/features/event-script.en.md): Combine text, key, mouse, and loop operations.

See the [English User Manual](docs/index.en.md) for usage instructions and complete command documentation.

## Download

Flori Input provides Windows x64 installer and portable packages on [GitHub Releases](https://github.com/flowersauce/Flori-Input/releases).

You can also install or update it with winget:

```powershell
winget install --id Flowersauce.FSClicker --exact --source winget
winget upgrade --id Flowersauce.FSClicker --exact --source winget
```

The package identifier remains `Flowersauce.FSClicker`; the application is named Flori Input. Starting with 1.4.0,
the winget manifest declares `Microsoft.VCRedist.2015+.x64` as a dependency, so winget handles runtime installation.
Updates may reach winget after the GitHub Release; check Releases for the latest version.

**For direct installer or portable downloads**, first install the latest [Microsoft Visual C++ v14 Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe).
Neither download bundles the VC++ runtime. If the latest x64 runtime is already installed, you do not need to install it again.

See [Installation and Updates](docs/getting-started/installation.en.md) for setup, updates, and configuration locations.

## Build

The project uses C++23, Slint 1.17.1, and Win32. Building requires Windows, CMake 3.30+, Visual Studio C++ x64 build
tools, Ninja, Slint C++ SDK, and vcpkg (Glaze 8.3.0).

In a VS x64 developer environment, set `VCPKG_ROOT` and `SLINT_ROOT` to your local vcpkg and Slint SDK root directories,
then install manifest dependencies:

```powershell
& "$env:VCPKG_ROOT/vcpkg.exe" install --triplet x64-windows --x-install-root="$PWD/vcpkg_installed"
```

Configuration example:

```powershell
cmake -S . -B build/Release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  "-DVCPKG_INSTALLED_DIR=$PWD/vcpkg_installed" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DVCPKG_MANIFEST_INSTALL=OFF `
  -DCMAKE_PREFIX_PATH="$env:SLINT_ROOT"
```

Build:

```powershell
cmake --build build/Release
```

## Packaging

Application icons are generated from a single SVG source. After changing the logo, follow
[Icon generation](tools/assets/README.md) to update the ICO and rebuild. A standalone PNG is optional.

Generate all three packages together: the portable ZIP, MSI and `Flori-Input-<version>-SHA256SUMS.txt` go in
`output/public`, while the unsigned Store MSIX goes in `output/store`. Upload only the three files in `public` to
GitHub Releases; submit the Store package separately.
Prepare the project Python environment, [WiX 7.0.0 and extensions](tools/packaging/windows/README.md),
and the [Windows SDK](tools/packaging/msix/README.md), then run:

```powershell
uv run --locked python tools/release/package_app.py
```

The script reads from `build/Release` by default and does not compile. It replaces existing local artifacts only after
all three packages are created, then removes artifacts matching the release naming rules from the output root and
older artifacts from `public` and `store`. Other files and subdirectories are preserved.
Override paths with `--build-dir`, `--output-dir`, `--wix` or `--sdk-bin`; a custom output root also uses `public` and `store` subdirectories.

The checksum file lists only the ZIP and MSI hashes and filenames. MSIX is excluded and has no separate checksum file.
After signing the MSI, run `uv run --locked python tools/release/generate_checksums.py` to refresh the checksum file
in `output/public` without repackaging.
This also removes the corresponding legacy per-package `.sha256` files.

The portable ZIP includes `portable.flag` and stores configuration and scripts next to the executable.
Development builds without this marker use `%LOCALAPPDATA%\Flori-Input`, with diagnostic logs
in the data directory's `logs` subdirectory.

The development branch now supports a native MSI shared by direct downloads and the winget community source.
Prepare WiX 7.0.0 with its UI and Util extensions and review its license as described in
[Windows MSI packaging and validation](tools/packaging/windows/README.md).

`uv run --locked python tools/release/release.py` generates all three packages and winget manifests using metadata from the actual MSI.
Manifests are written to `output/winget-manifests`.
It does not compile or upload. MSI uses the program directory `%LOCALAPPDATA%\Programs\Flori-Input`.
Direct downloads and winget offer no custom directory option during installation, upgrade or repair, including
silent installation. Upgrades and repairs retain the registered directory without allowing a location change;
use the portable ZIP if you need to choose the program location.
**Uninstalling the new MSI deletes all configuration, scripts, logs and preview data in
`%LOCALAPPDATA%\Flori-Input`. Copy anything you want to keep outside that directory first.** Upgrades and repairs
preserve data.

Store MSIX packaging includes the assigned package identity, Desktop VCLibs dependency and generated icons,
and runs alongside MSI and ZIP packaging by default.
Local sideloading uses a separately signed copy in `output/testing`; see [MSIX packaging and validation](tools/packaging/msix/README.md)
for the steps. Store certification and publication are still pending; these development artifacts have not been released.

Before release, verify the application runs and installs correctly on the target Windows versions. Configuration and script storage
locations are described in [Installation and Updates](docs/getting-started/installation.en.md).

## License

Copyright © 2024 Flowersauce

Flori Input uses the [MIT License](LICENSE). Third-party components are listed in
the [License Information](docs/legal/third-party.en.md).

<a href="https://slint.dev"><img alt="#MadeWithSlint" src="https://raw.githubusercontent.com/slint-ui/slint/master/logo/MadeWithSlint-logo-whitebg.png" height="24"></a>
