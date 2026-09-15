#!/usr/bin/env python3
"""
Generate winget manifests from the current release artifacts.

The script writes the three manifest files for the detected release version
under:

    output/winget-manifests/manifests/f/Flowersauce/FSClicker/<version>/

Default behavior assumes a completed release package in output/release.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

APP_NAME = "Flori-Input"
DISPLAY_NAME = "Flori Input"
PUBLISHER = "Flowersauce"
PACKAGE_ID_SUFFIX = "FSClicker"  # 保留已发布的安装身份，避免破坏升级路径。
PACKAGE_IDENTIFIER = f"{PUBLISHER}.{PACKAGE_ID_SUFFIX}"
WINDOWS_X64_SUFFIX = "windows-x64"
MANIFEST_VERSION = "1.12.0"
MANIFEST_SCHEMA_VERSION = "1.12.0"
DEFAULT_LOCALE = "en-US"
REPO_URL = "https://github.com/flowersauce/Flori-Input"


def project_root() -> Path:
    return Path(__file__).resolve().parents[2]


def parse_args() -> argparse.Namespace:
    root = project_root()
    parser = argparse.ArgumentParser(description="Generate winget manifests for Flori Input release artifacts.")
    parser.add_argument("--release-dir", default=str(root / "output" / "release"),
                        help="Directory containing the packaged release artifacts.")
    parser.add_argument("--output-dir", default=str(root / "output" / "winget-manifests"),
                        help="Root directory for generated winget manifests.")
    parser.add_argument("--version", default=None,
                        help="Override the detected version. Defaults to extracting it from the setup.exe name.")
    parser.add_argument("--installer-url", default=None,
                        help="Override the installer download URL.")
    parser.add_argument("--release-notes-url", default=None,
                        help="Override the release notes URL.")
    parser.add_argument("--installer-sha256", default=None,
                        help="Override the installer SHA256 value.")
    parser.add_argument("--package-url", default=REPO_URL, help="Package homepage URL.")
    parser.add_argument("--publisher-url", default="https://github.com/flowersauce", help="Publisher homepage URL.")
    parser.add_argument("--publisher-support-url", default=f"{REPO_URL}/issues",
                        help="Publisher support URL.")
    parser.add_argument("--license-url", default=f"{REPO_URL}/blob/main/LICENSE", help="License URL.")
    return parser.parse_args()


def detect_setup_artifact(release_dir: Path) -> Path:
    pattern = re.compile(rf"^{re.escape(APP_NAME)}-v(.+)-{re.escape(WINDOWS_X64_SUFFIX)}-setup\.exe$", re.IGNORECASE)
    candidates = []
    for path in release_dir.glob(f"{APP_NAME}-v*-{WINDOWS_X64_SUFFIX}-setup.exe"):
        if pattern.match(path.name):
            candidates.append(path)

    if not candidates:
        raise FileNotFoundError(
            f"Could not find {APP_NAME}-v*-{WINDOWS_X64_SUFFIX}-setup.exe in {release_dir}."
        )

    return max(candidates, key=lambda item: item.stat().st_mtime)


def extract_version(setup_path: Path, explicit_version: str | None) -> str:
    if explicit_version:
        return explicit_version

    pattern = re.compile(rf"^{re.escape(APP_NAME)}-v(.+)-{re.escape(WINDOWS_X64_SUFFIX)}-setup\.exe$", re.IGNORECASE)
    match = pattern.match(setup_path.name)
    if not match:
        raise ValueError(f"Cannot infer version from installer name: {setup_path.name}")
    return match.group(1)


def read_or_compute_sha256(setup_path: Path, explicit_sha256: str | None) -> str:
    if explicit_sha256:
        return explicit_sha256.strip()

    sha_path = setup_path.with_suffix(setup_path.suffix + ".sha256")
    if sha_path.exists():
        content = sha_path.read_text(encoding="utf-8", errors="ignore").strip()
        if content:
            return content.split()[0]

    digest = hashlib.sha256(setup_path.read_bytes()).hexdigest()
    return digest


def release_url(version: str) -> str:
    return f"{REPO_URL}/releases/download/v{version}/{APP_NAME}-v{version}-{WINDOWS_X64_SUFFIX}-setup.exe"


def notes_url(version: str) -> str:
    return f"{REPO_URL}/releases/tag/v{version}"


def manifest_root(output_dir: Path, version: str) -> Path:
    return output_dir / "manifests" / "f" / PUBLISHER / PACKAGE_ID_SUFFIX / version


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def schema_header(manifest_kind: str) -> str:
    return (
        "# yaml-language-server: "
        f"$schema=https://aka.ms/winget-manifest.{manifest_kind}.{MANIFEST_SCHEMA_VERSION}.schema.json\n\n"
    )


def build_version_manifest(version: str) -> str:
    return schema_header("version") + (
        f"PackageIdentifier: {PACKAGE_IDENTIFIER}\n"
        f"PackageVersion: {version}\n"
        f"DefaultLocale: {DEFAULT_LOCALE}\n"
        f"ManifestType: version\n"
        f"ManifestVersion: {MANIFEST_VERSION}\n"
    )


def build_installer_manifest(version: str, installer_url: str, installer_sha256: str) -> str:
    return schema_header("installer") + (
        f"PackageIdentifier: {PACKAGE_IDENTIFIER}\n"
        f"PackageVersion: {version}\n"
        f"InstallerType: exe\n"
        f"Scope: user\n"
        f"InstallModes:\n"
        f"- silent\n"
        f"- silentWithProgress\n"
        f"InstallerSwitches:\n"
        f"  Silent: --silent\n"
        f"  SilentWithProgress: --silent\n"
        f"UpgradeBehavior: install\n"
        f"ProductCode: {PACKAGE_IDENTIFIER}\n"
        f"AppsAndFeaturesEntries:\n"
        f"- DisplayName: {DISPLAY_NAME}\n"
        f"  DisplayVersion: {version}\n"
        f"  Publisher: {PUBLISHER}\n"
        f"  ProductCode: {PACKAGE_IDENTIFIER}\n"
        f"  InstallerType: exe\n"
        f"Installers:\n"
        f"- Architecture: x64\n"
        f"  InstallerUrl: {installer_url}\n"
        f"  InstallerSha256: {installer_sha256}\n"
        f"  ProductCode: {PACKAGE_IDENTIFIER}\n"
        f"ManifestType: installer\n"
        f"ManifestVersion: {MANIFEST_VERSION}\n"
    )


def build_locale_manifest(version: str, package_url: str, publisher_url: str, publisher_support_url: str,
                         license_url: str, release_notes_url: str) -> str:
    return schema_header("defaultLocale") + (
        f"PackageIdentifier: {PACKAGE_IDENTIFIER}\n"
        f"PackageVersion: {version}\n"
        f"PackageLocale: {DEFAULT_LOCALE}\n"
        f"Publisher: {PUBLISHER}\n"
        f"PublisherUrl: {publisher_url}\n"
        f"PublisherSupportUrl: {publisher_support_url}\n"
        f"PackageName: {DISPLAY_NAME}\n"
        f"PackageUrl: {package_url}\n"
        f"License: MIT\n"
        f"LicenseUrl: {license_url}\n"
        f"ShortDescription: A lightweight input automation tool for Windows.\n"
        f"Tags:\n"
        f"- auto-clicker\n"
        f"- clicker\n"
        f"- input\n"
        f"- windows\n"
        f"ReleaseNotesUrl: {release_notes_url}\n"
        f"ManifestType: defaultLocale\n"
        f"ManifestVersion: {MANIFEST_VERSION}\n"
    )


def main() -> None:
    args = parse_args()
    release_dir = Path(args.release_dir).resolve()
    output_dir = Path(args.output_dir).resolve()

    if not release_dir.exists():
        raise FileNotFoundError(f"Release directory does not exist: {release_dir}")

    setup_path = detect_setup_artifact(release_dir)
    version = extract_version(setup_path, args.version)
    installer_url = args.installer_url or release_url(version)
    release_notes_url = args.release_notes_url or notes_url(version)
    installer_sha256 = read_or_compute_sha256(setup_path, args.installer_sha256)

    target_dir = manifest_root(output_dir, version)
    target_dir.mkdir(parents=True, exist_ok=True)

    write_text(target_dir / f"{PACKAGE_IDENTIFIER}.yaml", build_version_manifest(version))
    write_text(target_dir / f"{PACKAGE_IDENTIFIER}.installer.yaml",
               build_installer_manifest(version, installer_url, installer_sha256))
    write_text(target_dir / f"{PACKAGE_IDENTIFIER}.locale.en-US.yaml",
               build_locale_manifest(
                   version,
                   args.package_url,
                   args.publisher_url,
                   args.publisher_support_url,
                   args.license_url,
                   release_notes_url,
               ))

    print(f"Generated winget manifests in: {target_dir}")
    print(f"Installer SHA256: {installer_sha256}")


if __name__ == "__main__":
    main()
