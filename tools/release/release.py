#!/usr/bin/env python3
"""
@file release.py
@brief Flori Input 发布流程管理脚本。

负责执行发布前检查、应用打包以及 winget 清单生成。
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

APP_NAME = "Flori-Input"
PLATFORM = "windows-x64"

ROOT_DIR = Path(__file__).resolve().parents[2]

PACKAGE_SCRIPT = ROOT_DIR / "tools" / "release" / "package_app.py"
WINGET_SCRIPT = ROOT_DIR / "tools" / "release" / "generate_winget_manifest.py"

CMAKE_FILE = ROOT_DIR / "CMakeLists.txt"
VCPKG_FILE = ROOT_DIR / "vcpkg.json"

OUTPUT_DIR = ROOT_DIR / "output" / "release"


def run_command(command: list[str]) -> None:
    """
    @brief 执行外部命令。

    @param command 命令参数列表。
    """
    print(f"\n> {' '.join(command)}")

    subprocess.run(
        command,
        cwd=ROOT_DIR,
        check=True,
    )


def read_cmake_version() -> str:
    """
    @brief 从 CMakeLists.txt 读取项目版本。

    @return 项目版本号。
    """
    content = CMAKE_FILE.read_text(
        encoding="utf-8"
    )

    match = re.search(
        r"set\s*\(\s*FLORI_INPUT_VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\s*\)",
        content,
    )

    if not match:
        raise RuntimeError(
            "无法从 CMakeLists.txt 中读取项目版本"
        )

    return match.group(1)


def read_vcpkg_version() -> str:
    """
    @brief 从 vcpkg.json 读取项目版本。

    @return 项目版本号。
    """
    data = json.loads(
        VCPKG_FILE.read_text(
            encoding="utf-8"
        )
    )

    version = data.get("version")

    if not version:
        raise RuntimeError(
            "无法从 vcpkg.json 中读取项目版本"
        )

    return version


def check_version() -> str:
    """
    @brief 检查项目版本一致性。

    当前检查：
    - CMakeLists.txt
    - vcpkg.json

    @return 当前项目版本。
    """
    cmake_version = read_cmake_version()
    vcpkg_version = read_vcpkg_version()

    print("[版本检查]")
    print(f"CMake 版本: {cmake_version}")
    print(f"vcpkg 版本: {vcpkg_version}")

    if cmake_version != vcpkg_version:
        raise RuntimeError(
            "CMakeLists.txt 与 vcpkg.json 的版本不一致"
        )

    print("版本检查通过")

    return cmake_version


def find_setup(version: str) -> Path:
    """
    @brief 查找 Velopack 生成的安装程序。

    @param version 项目版本。
    @return 安装程序路径。
    """
    setup_name = (
        f"{APP_NAME}-v{version}-{PLATFORM}-setup.exe"
    )

    setup_path = OUTPUT_DIR / setup_name

    if not setup_path.is_file():
        raise FileNotFoundError(
            f"未找到安装程序: {setup_path}"
        )

    return setup_path


def main() -> None:
    """
    @brief 执行完整发布流程。
    """
    version = check_version()

    print(
        f"\n准备发布 Flori Input {version}"
    )

    print(
        "\n[1/2] 生成发布包"
    )

    run_command(
        [
            sys.executable,
            str(PACKAGE_SCRIPT),
            "--with-velopack",
        ]
    )

    setup_path = find_setup(version)

    print(
        f"安装程序: {setup_path.name}"
    )

    print(
        "\n[2/2] 生成 winget 清单"
    )

    run_command(
        [
            sys.executable,
            str(WINGET_SCRIPT),
        ]
    )

    print(
        "\n发布流程完成"
    )


if __name__ == "__main__":
    main()
