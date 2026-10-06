#!/usr/bin/env python3
"""从现有 Release 构建生成 Flori Input 的 Windows 发布包。"""

from __future__ import annotations

import argparse
import re
import shutil
import tempfile
import zipfile
from pathlib import Path

from generate_checksums import checksum_filename, write_checksums
from msi_package import create_msi
from msix_package import create_msix


APP_NAME = "Flori-Input"
EXE_NAME = f"{APP_NAME}.exe"
PLATFORM = "windows-x64"


def write_third_party_notices(destination: Path, root: Path) -> None:
    """将仓库内许可原文合并为三个发布包共用的单一声明文件。"""
    sources = (
        ("Slint 1.17.1 - Royalty-free License 2.0", "Slint-Royalty-free-2.0.md",
         "https://github.com/slint-ui/slint/blob/v1.17.1/LICENSES/LicenseRef-Slint-Royalty-free-2.0.md"),
        ("Slint 1.17.1 SDK - Third-party dependencies", "Slint-1.17.1-THIRDPARTY.md",
         "https://github.com/slint-ui/slint/releases/tag/v1.17.1"),
        ("Glaze 8.3.0 - MIT License", "Glaze-8.3.0-LICENSE.txt",
         "https://github.com/stephenberry/glaze/blob/v8.3.0/LICENSE"),
        ("Jura - SIL Open Font License 1.1", "Jura-OFL.txt",
         "https://github.com/google/fonts/blob/main/ofl/jura/OFL.txt"),
        ("Sarasa Gothic - SIL Open Font License 1.1", "Sarasa-Gothic-LICENSE.txt",
         "https://github.com/be5invis/Sarasa-Gothic/blob/main/LICENSE"),
        ("Slint logo assets - CC BY-ND 4.0", "CC-BY-ND-4.0.txt",
         "https://github.com/slint-ui/slint/blob/v1.17.1/REUSE.toml"),
    )
    sections = [
        "Flori Input - Third-party notices\n\n"
        "Flori Input's own MIT license is provided in LICENSE. The components\n"
        "and assets below retain their respective licenses.\n\n"
        "The embedded FloriInputUI font is a derivative combining Jura and\n"
        "Sarasa UI SC glyphs, distributed under SIL Open Font License 1.1.\n\n"
        "Slint logo assets: Copyright (c) SixtyFPS GmbH, CC BY-ND 4.0.\n"
        "Source: https://github.com/slint-ui/slint/tree/v1.17.1/logo\n\n"
        "The Slint SDK dependency notices are retained as supplied by its\n"
        "1.17.1 distribution and may cover optional/platform-specific features.\n"
    ]
    for title, filename, source in sources:
        path = root / "resources" / "licenses" / filename
        content = path.read_text(encoding="utf-8")
        if not content.strip():
            raise ValueError(f"第三方许可原文为空，无法打包：{path}")
        sections.append(f"{'=' * 72}\n{title}\nSource: {source}\n\n{content.rstrip()}\n")
    destination.write_text("\n".join(sections), encoding="utf-8", newline="\n")


def project_root() -> Path:
    return Path(__file__).resolve().parents[2]


def parse_args() -> argparse.Namespace:
    root = project_root()
    parser = argparse.ArgumentParser(description="将现有 Slint/MSVC Release 构建统一打包为便携 ZIP、MSI 和商店 MSIX。")
    parser.add_argument("--build-dir", type=Path, default=root / "build" / "Release",
                        help="CMake Release 构建目录，默认 build/Release。")
    parser.add_argument("--output-dir", type=Path, default=root / "output",
                        help="发布产物目录，默认 output。全部制作成功后覆盖同名产物并清理旧版发布包。")
    parser.add_argument("--wix", default="wix",
                        help="WiX 7.0.0 可执行文件路径或 PATH 中的命令。")
    parser.add_argument("--sdk-bin", type=Path,
                        help="Windows SDK x64 工具目录；默认查找本机已安装的最新 SDK。")
    return parser.parse_args()


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
    source_version = re.search(r"set\s*\(\s*FLORI_INPUT_VERSION\s+(\d+\.\d+\.\d+)\s*\)",
                               (root / "CMakeLists.txt").read_text(encoding="utf-8"))
    if source_version is None or source_version.group(1) != version:
        raise ValueError("Release 构建版本与当前源码版本不一致；请重新配置并构建后再打包。")

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
    write_third_party_notices(destination / "THIRD-PARTY-NOTICES.txt", root)


def create_portable_zip(application_dir: Path, destination: Path) -> None:
    with zipfile.ZipFile(destination, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr(f"{application_dir.name}/portable.flag", "Flori Input portable data mode.\n")
        for file in sorted(path for path in application_dir.rglob("*") if path.is_file()):
            archive.write(file, file.relative_to(application_dir.parent))


def remove_stale_packages(output_dir: Path, artifacts: tuple[Path, ...]) -> None:
    """新产物输出后，只清理本目录中按发布命名规则生成的旧包及校验文件。"""
    current_names = {artifact.name for artifact in artifacts}
    pattern = re.compile(
        rf"{re.escape(APP_NAME)}-v\d+\.\d+\.\d+-"
        rf"(?:{re.escape(PLATFORM)}-(?:portable\.zip|setup\.msi|store\.msix)(?:\.sha256)?|SHA256SUMS\.txt)"
    )
    removed = 0
    for path in output_dir.iterdir():
        if path.is_file() and pattern.fullmatch(path.name) and path.name not in current_names:
            path.unlink()
            removed += 1
    if removed:
        print(f"已清理 {removed} 个旧版发布包或校验文件。")


def main() -> None:
    args = parse_args()
    root = project_root()
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()
    executable, version = release_build(build_dir, root)
    base_name = f"{APP_NAME}-v{version}-{PLATFORM}"
    portable_name = f"{base_name}-portable"
    portable_zip = output_dir / f"{portable_name}.zip"
    setup_msi = output_dir / f"{base_name}-setup.msi"
    store_msix = output_dir / f"{base_name}-store.msix"
    checksum_path = output_dir / checksum_filename(version)

    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="flori-input-", dir=output_dir) as temporary:
        work_dir = Path(temporary)
        application_dir = work_dir / portable_name
        stage_application(executable, application_dir, root)

        temporary_zip = work_dir / portable_zip.name
        create_portable_zip(application_dir, temporary_zip)

        temporary_setup = work_dir / setup_msi.name
        create_msi(application_dir, temporary_setup, version, args.wix, root)

        temporary_msix = work_dir / store_msix.name
        create_msix(application_dir, temporary_msix, version, args.sdk_bin, root)
        temporary_checksums = work_dir / checksum_path.name
        write_checksums((temporary_zip, temporary_setup, temporary_msix), temporary_checksums)

        temporary_zip.replace(portable_zip)
        print(f"便携包：{portable_zip}")
        temporary_setup.replace(setup_msi)
        print(f"安装包：{setup_msi}")
        temporary_msix.replace(store_msix)
        print(f"商店包（未签名）：{store_msix}")
        temporary_checksums.replace(checksum_path)
        print(f"统一校验文件：{checksum_path}")

    remove_stale_packages(output_dir, (portable_zip, setup_msi, store_msix, checksum_path))


if __name__ == "__main__":
    main()
