#!/usr/bin/env python3
"""为现有公开发布 ZIP 和 MSI 生成统一 SHA256 校验清单，不重新打包。"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


APP_NAME = "Flori-Input"
PLATFORM = "windows-x64"


def checksum_filename(version: str) -> str:
    return f"{APP_NAME}-{version}-SHA256SUMS.txt"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_checksums(packages: tuple[Path, ...], destination: Path) -> Path:
    """使用包的最终内容生成清单，每行包含哈希和文件名。"""
    lines = [f"{sha256_file(path)}  {path.name}\n" for path in sorted(packages)]
    destination.write_text("".join(lines), encoding="utf-8", newline="\n")
    return destination


def read_checksums(path: Path) -> dict[str, str]:
    """按文件名读取校验值；无效行和重复文件名会报错。"""
    checksums: dict[str, str] = {}
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        if not line.strip():
            continue
        match = re.fullmatch(r"([0-9a-fA-F]{64})  (.+)", line)
        if match is None:
            raise ValueError(f"Invalid SHA256 checksum entry: {path}:{number}")
        digest, filename = match.groups()
        if filename in checksums:
            raise ValueError(f"Duplicate checksum filename: {filename}")
        checksums[filename] = digest
    return checksums


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-dir", type=Path, default=Path(__file__).resolve().parents[2] / "output" / "public",
                        help="现有公开发布 ZIP 和 MSI 所在目录，默认 output/public。")
    parser.add_argument("--version", help="指定发布版本；默认从目录中唯一的 MSI 文件名读取。")
    args = parser.parse_args()
    release_dir = args.release_dir.resolve()
    version = args.version
    if version is None:
        installers = sorted(path for path in release_dir.glob(f"{APP_NAME}-[0-9]*-{PLATFORM}-setup.msi")
                            if path.is_file())
        if len(installers) != 1:
            parser.error("请保留一个 MSI，或通过 --version 指定版本。")
        match = re.fullmatch(rf"{re.escape(APP_NAME)}-(\d+\.\d+\.\d+)-{re.escape(PLATFORM)}-setup\.msi",
                             installers[0].name)
        if match is None:
            parser.error("无法从 MSI 文件名读取三段数字版本。")
        version = match.group(1)
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        parser.error("版本必须为三段数字，例如 1.5.0。")

    base_name = f"{APP_NAME}-{version}-{PLATFORM}"
    packages = tuple(release_dir / f"{base_name}-{suffix}"
                     for suffix in ("portable.zip", "setup.msi"))
    checksum_path = write_checksums(packages, release_dir / checksum_filename(version))
    for package in packages:
        package.with_suffix(package.suffix + ".sha256").unlink(missing_ok=True)
    print(f"公开发布校验文件：{checksum_path}")


if __name__ == "__main__":
    main()
