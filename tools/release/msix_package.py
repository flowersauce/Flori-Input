"""从现有 Release 文件生成未签名商店 MSIX，使用项目图标工具和 Windows SDK。"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SDK_TOOLS = ("makeappx.exe", "makepri.exe")


def msix_version(version: str) -> str:
    """商店四段版本号保留末段为 0，拒绝预发布标签和超范围字段。"""
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError(f"MSIX 需要三段数字项目版本：{version!r}")
    fields = tuple(map(int, version.split(".")))
    if fields[0] == 0 or any(field > 65535 for field in fields):
        raise ValueError(f"MSIX 版本号超出商店允许范围：{version}")
    return ".".join(map(str, (*fields, 0)))


def sdk_bin_directory(override: Path | None) -> Path:
    """使用显式 SDK x64 工具目录，或查找本机已安装的最新 Windows 10 SDK。"""
    if os.name != "nt":
        raise OSError("MSIX 制作需要 Windows SDK。")
    if override is not None:
        candidate = override.resolve()
        if not all((candidate / name).is_file() for name in SDK_TOOLS):
            raise FileNotFoundError(f"SDK 工具目录缺少 MakeAppx 或 MakePri：{candidate}")
        return candidate

    program_files = os.environ.get("ProgramFiles(x86)")
    if not program_files:
        raise FileNotFoundError("找不到 Windows SDK，请用 --sdk-bin 指定其 x64 工具目录。")
    sdk_root = Path(program_files) / "Windows Kits" / "10" / "bin"
    if not sdk_root.is_dir():
        raise FileNotFoundError(f"找不到 Windows SDK：{sdk_root}；可使用 --sdk-bin。")
    candidates = sorted(
        (directory for directory in sdk_root.iterdir()
         if directory.is_dir() and re.fullmatch(r"10\.0\.\d+\.\d+", directory.name)),
        key=lambda directory: tuple(map(int, directory.name.split("."))),
        reverse=True,
    )
    for directory in candidates:
        candidate = directory / "x64"
        if all((candidate / name).is_file() for name in SDK_TOOLS):
            return candidate
    raise FileNotFoundError("未找到包含 MakeAppx 和 MakePri 的 SDK；请使用 --sdk-bin。")


def create_msix(application_dir: Path, destination: Path, version: str,
                sdk_bin: Path | None, root: Path) -> None:
    """暂存商店清单和图标、生成 PRI，再调用 MakeAppx；不签名或安装。"""
    package_version = msix_version(version)
    tools = sdk_bin_directory(sdk_bin)
    if any(not file.is_file() or file.name.casefold() == "portable.flag"
           for file in application_dir.iterdir()):
        raise ValueError("MSIX 内容只能包含应用文件，不得含便携标记或数据目录。")

    work_dir = destination.parent / "msix-source"
    work_dir.mkdir()
    payload = work_dir / "package"
    shutil.copytree(application_dir, payload)
    packaging_dir = root / "tools" / "packaging" / "msix"
    template = (packaging_dir / "AppxManifest.xml.in").read_text(encoding="utf-8")
    if template.count("@APP_VERSION@") != 1:
        raise ValueError("MSIX 清单必须包含一个 @APP_VERSION@ 版本占位符。")
    (payload / "AppxManifest.xml").write_text(
        template.replace("@APP_VERSION@", package_version), encoding="utf-8")

    subprocess.run([
        sys.executable, str(root / "tools" / "assets" / "generate_icons.py"),
        "--source-svg", str(root / "resources" / "icons" / "Flori-Input.svg"),
        "--msix-assets", str(payload / "Assets"),
    ], cwd=root, check=True)

    pri_config = work_dir / "priconfig.xml"
    subprocess.run([
        str(tools / "makepri.exe"), "createconfig", "/cf", str(pri_config),
        "/dq", "en-US", "/pv", "10.0.0", "/o",
    ], cwd=root, check=True)
    subprocess.run([
        str(tools / "makepri.exe"), "new", "/pr", str(payload), "/cf", str(pri_config),
        "/of", str(payload / "resources.pri"), "/o",
    ], cwd=root, check=True)
    subprocess.run([
        str(tools / "makeappx.exe"), "pack", "/d", str(payload), "/p", str(destination),
        "/h", "SHA256", "/o",
    ], cwd=root, check=True)
