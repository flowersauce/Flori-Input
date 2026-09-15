#!/usr/bin/env python3
"""从现有 Release 构建生成 Flori Input 的 Windows 发布包。"""

from __future__ import annotations

import argparse
import hashlib
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path


APP_NAME = "Flori-Input"
DISPLAY_NAME = "Flori Input"
AUTHOR = "Flowersauce"
EXE_NAME = f"{APP_NAME}.exe"
PLATFORM = "windows-x64"
PACK_ID = "Flowersauce.FSClicker"  # 已发布的安装身份，不能随展示名一起更改。


def project_root() -> Path:
    return Path(__file__).resolve().parents[2]


def parse_args() -> argparse.Namespace:
    root = project_root()
    parser = argparse.ArgumentParser(description="将现有 Slint/MSVC Release 构建打包为便携 ZIP 和可选安装器。")
    parser.add_argument("--build-dir", type=Path, default=root / "build" / "Release",
                        help="CMake Release 构建目录，默认 build/Release。")
    parser.add_argument("--output-dir", type=Path, default=root / "output" / "release",
                        help="发布产物目录，默认 output/release。已有同名产物会覆盖。")
    parser.add_argument("--with-velopack", action="store_true", help="额外生成 Velopack 安装器。")
    parser.add_argument("--keep-velopack-feed", action="store_true",
                        help="保留 Velopack 更新源文件（需同时传入 --with-velopack）。")
    parser.add_argument("--vpk", default="vpk", help="vpk 可执行文件或 PATH 中的命令，默认 vpk。")
    args = parser.parse_args()
    if args.keep_velopack_feed and not args.with_velopack:
        parser.error("--keep-velopack-feed 需要 --with-velopack")
    return args


def read_cmake_cache(build_dir: Path) -> dict[str, str]:
    cache_file = build_dir / "CMakeCache.txt"
    if not cache_file.is_file():
        raise FileNotFoundError(f"缺少 CMakeCache.txt：{cache_file}；请指定实际的 Release 构建目录。")

    values: dict[str, str] = {}
    for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(("//", "#")) or "=" not in line:
            continue
        key_and_type, value = line.split("=", 1)
        values[key_and_type.split(":", 1)[0]] = value
    return values


def release_build(build_dir: Path, root: Path) -> tuple[Path, str]:
    cache = read_cmake_cache(build_dir)
    source_directory = cache.get("CMAKE_HOME_DIRECTORY")
    if not source_directory or Path(source_directory).resolve() != root:
        raise ValueError("构建缓存指向其他源目录；请为当前仓库重新配置 Release 构建。")
    if cache.get("CMAKE_PROJECT_NAME") != APP_NAME:
        raise ValueError("构建缓存仍属于旧项目；请重新配置 Flori-Input 的 Release 构建。")

    build_type = cache.get("CMAKE_BUILD_TYPE", "")
    if build_type and build_type.casefold() != "release":
        raise ValueError(f"拒绝打包非 Release 构建：CMAKE_BUILD_TYPE={build_type}")
    if not build_type and "CMAKE_CONFIGURATION_TYPES" not in cache:
        raise ValueError("无法从 CMake 缓存确认 Release 配置。")

    version = cache.get("CMAKE_PROJECT_VERSION", "")
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?", version):
        raise ValueError(f"CMake 缓存中的版本号无效：{version!r}")

    executable = build_dir / (EXE_NAME if build_type else f"Release/{EXE_NAME}")
    if not executable.is_file():
        raise FileNotFoundError(f"缺少 {executable}；请先重新构建 FloriInput 的 Release 配置。")
    return executable, version


def stage_application(executable: Path, destination: Path, root: Path) -> None:
    destination.mkdir()
    shutil.copy2(executable, destination / EXE_NAME)

    dlls = sorted(path for path in executable.parent.iterdir() if path.is_file() and path.suffix.casefold() == ".dll")
    if not any(path.name.casefold() == "slint_cpp.dll" for path in dlls):
        raise FileNotFoundError("构建输出缺少 slint_cpp.dll；请重新构建并确认 CMake 的运行时复制步骤已完成。")
    for dll in dlls:
        shutil.copy2(dll, destination / dll.name)

    license_file = root / "LICENSE"
    if not license_file.is_file():
        raise FileNotFoundError("缺少 LICENSE，无法生成发布包。")
    shutil.copy2(license_file, destination / "LICENSE")


def create_portable_zip(application_dir: Path, destination: Path) -> None:
    with zipfile.ZipFile(destination, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(path for path in application_dir.rglob("*") if path.is_file()):
            archive.write(file, file.relative_to(application_dir.parent))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_checksum(path: Path) -> Path:
    checksum = path.with_suffix(path.suffix + ".sha256")
    checksum.write_text(f"{sha256_file(path)}  {path.name}\n", encoding="utf-8")
    return checksum


def create_velopack_setup(application_dir: Path, output_dir: Path, version: str, vpk: str, root: Path) -> Path:
    icon = root / "resources" / "icons" / "Flori-Input.ico"
    if not icon.is_file():
        raise FileNotFoundError(f"缺少安装器图标：{icon}")
    if not shutil.which(vpk) and not Path(vpk).is_file():
        raise FileNotFoundError(f"找不到 vpk：{vpk}")

    command = [
        vpk, "pack", "--packId", PACK_ID, "--packVersion", version,
        "--packDir", str(application_dir), "--mainExe", EXE_NAME,
        "--packTitle", DISPLAY_NAME, "--packAuthors", AUTHOR,
        "--icon", str(icon), "--outputDir", str(output_dir),
    ]
    subprocess.run(command, check=True)
    setups = [path for path in output_dir.iterdir() if path.is_file() and path.name.casefold().endswith("setup.exe")]
    if len(setups) != 1:
        raise RuntimeError(f"Velopack 应生成一个 Setup.exe，实际找到 {len(setups)} 个。")
    return setups[0]


def main() -> None:
    args = parse_args()
    root = project_root()
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()
    executable, version = release_build(build_dir, root)
    base_name = f"{APP_NAME}-v{version}-{PLATFORM}"
    portable_name = f"{base_name}-portable"
    portable_zip = output_dir / f"{portable_name}.zip"
    setup_exe = output_dir / f"{base_name}-setup.exe"
    feed_dir = output_dir / f"{base_name}-velopack-feed"

    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="flori-input-", dir=output_dir) as temporary:
        work_dir = Path(temporary)
        application_dir = work_dir / portable_name
        stage_application(executable, application_dir, root)

        temporary_zip = work_dir / portable_zip.name
        create_portable_zip(application_dir, temporary_zip)
        write_checksum(temporary_zip)

        if args.with_velopack:
            vpk_output = work_dir / "velopack"
            vpk_output.mkdir()
            generated_setup = create_velopack_setup(application_dir, vpk_output, version, args.vpk, root)
            temporary_setup = work_dir / setup_exe.name
            shutil.copy2(generated_setup, temporary_setup)
            write_checksum(temporary_setup)
            if args.keep_velopack_feed:
                if feed_dir.exists():
                    is_junction = getattr(feed_dir, "is_junction", lambda: False)()
                    if not feed_dir.is_dir() or feed_dir.is_symlink() or is_junction:
                        raise FileExistsError(f"无法覆盖非普通目录的更新源：{feed_dir}")
                    previous_feed = work_dir / "previous-velopack-feed"
                    feed_dir.rename(previous_feed)
                    try:
                        vpk_output.rename(feed_dir)
                    except OSError:
                        previous_feed.rename(feed_dir)
                        raise
                else:
                    vpk_output.rename(feed_dir)

        temporary_zip.replace(portable_zip)
        temporary_zip.with_suffix(".zip.sha256").replace(portable_zip.with_suffix(".zip.sha256"))
        print(f"便携包：{portable_zip}")
        if args.with_velopack:
            temporary_setup.replace(setup_exe)
            temporary_setup.with_suffix(".exe.sha256").replace(setup_exe.with_suffix(".exe.sha256"))
            print(f"安装包：{setup_exe}")
        if args.keep_velopack_feed:
            print(f"更新源：{feed_dir}")


if __name__ == "__main__":
    main()
