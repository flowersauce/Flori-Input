<p align="center"><img src="resources/icons/Flori-Input_transparent_dynamic.svg" alt="Flori Input" width="120"></p>

<h1 align="center">Flori Input</h1>

<p align="center">A Windows input automation tool supporting repeated input, simulated paste, and event scripts.</p>

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

Flori Input has not been officially released yet. The previous version FSClicker can be downloaded
from [GitHub Releases](https://github.com/flowersauce/Flori-Input/releases), or installed via winget:

```powershell
winget install Flowersauce.FSClicker
```

The package above still uses the old name and old features. To try the current development version, build it from source
using the instructions below.

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

Generate a portable package from an existing Release build. It reads from `build/Release` by default, does not build
automatically, and overwrites local artifacts with the same name:

```powershell
python tools/release/package_app.py
```

To generate an installer package, install the Velopack CLI (`vpk`) first, then run:

```powershell
python tools/release/package_app.py --with-velopack
```

Artifacts are located in `output/release`. Other build directories can be specified with `--build-dir`. Before release,
verify the application runs and installs correctly in a clean Windows environment. Configuration and script storage
locations are described in [Installation and Updates](docs/getting-started/installation.en.md).

## License

Copyright © 2024 Flowersauce

Flori Input uses the [MIT License](LICENSE). Third-party components are listed in
the [License Information](docs/legal/third-party.en.md).

<a href="https://slint.dev"><img alt="#MadeWithSlint" src="https://raw.githubusercontent.com/slint-ui/slint/master/logo/MadeWithSlint-logo-whitebg.png" height="24"></a>