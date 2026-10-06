#!/usr/bin/env python3
"""
Generate winget manifests from the current release artifacts.

The script writes the three manifest files for the detected release version
under:

    output/winget-manifests/manifests/f/Flowersauce/FSClicker/<version>/

Default behavior assumes a completed release package in output.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from generate_checksums import checksum_filename, read_checksums, sha256_file
from msi_package import MsiMetadata, read_msi_metadata

APP_NAME = "Flori-Input"
DISPLAY_NAME = "Flori Input"
PUBLISHER = "Flowersauce"
PACKAGE_ID_SUFFIX = "FSClicker"  # 保留已发布的 WinGet 包标识。
PACKAGE_IDENTIFIER = f"{PUBLISHER}.{PACKAGE_ID_SUFFIX}"
WINDOWS_X64_SUFFIX = "windows-x64"
VC_RUNTIME_PACKAGE = "Microsoft.VCRedist.2015+.x64"
MANIFEST_VERSION = "1.12.0"
MANIFEST_SCHEMA_VERSION = "1.12.0"
DEFAULT_LOCALE = "en-US"
REPO_URL = "https://github.com/flowersauce/Flori-Input"


def project_root() -> Path:
    return Path(__file__).resolve().parents[2]


def parse_args() -> argparse.Namespace:
    root = project_root()
    parser = argparse.ArgumentParser(description="Generate winget manifests for Flori Input release artifacts.")
    parser.add_argument("--release-dir", default=str(root / "output"),
                        help="Directory containing the packaged release artifacts.")
    parser.add_argument("--output-dir", default=str(root / "output" / "winget-manifests"),
                        help="Root directory for generated winget manifests.")
    parser.add_argument("--version", default=None,
                        help="Select a version and verify it against the MSI ProductVersion.")
    parser.add_argument("--installer", type=Path, default=None,
                        help="Use this MSI instead of detecting an artifact in --release-dir.")
    parser.add_argument("--installer-url", default=None,
                        help="Override the installer download URL.")
    parser.add_argument("--release-notes-url", default=None,
                        help="Override the release notes URL.")
    parser.add_argument("--installer-sha256", default=None,
                        help="Verify this expected SHA256 against the actual installer.")
    parser.add_argument("--package-url", default=REPO_URL, help="Package homepage URL.")
    parser.add_argument("--publisher-url", default="https://github.com/flowersauce", help="Publisher homepage URL.")
    parser.add_argument("--publisher-support-url", default=f"{REPO_URL}/issues",
                        help="Publisher support URL.")
    parser.add_argument("--license-url", default=f"{REPO_URL}/blob/HEAD/LICENSE", help="License URL.")
    return parser.parse_args()


def detect_setup_artifact(release_dir: Path, version: str | None) -> Path:
    candidates = sorted(release_dir.glob(f"{APP_NAME}-v*-{WINDOWS_X64_SUFFIX}-setup.msi"))
    candidates = [path for path in candidates if path.is_file()
                  and (version is None or path.name == f"{APP_NAME}-v{version}-{WINDOWS_X64_SUFFIX}-setup.msi")]
    if not candidates:
        raise FileNotFoundError(
            f"Could not find the requested Flori Input MSI in {release_dir}."
        )
    if len(candidates) != 1:
        raise ValueError("Multiple MSI releases found; select --version or --installer explicitly.")
    return candidates[0]


def extract_version(setup_path: Path, explicit_version: str | None, metadata: MsiMetadata) -> str:
    pattern = re.compile(rf"^{re.escape(APP_NAME)}-v(.+)-{re.escape(WINDOWS_X64_SUFFIX)}-setup\.msi$", re.IGNORECASE)
    match = pattern.match(setup_path.name)
    if not match:
        raise ValueError(f"Cannot infer version from installer name: {setup_path.name}")
    version = metadata.version
    if match.group(1) != version or (explicit_version and explicit_version != version):
        raise ValueError("The requested version, artifact name and MSI ProductVersion must match.")
    return version


def read_or_compute_sha256(setup_path: Path, explicit_sha256: str | None, version: str) -> str:
    actual_sha256 = sha256_file(setup_path).upper()
    expected_sha256 = []
    if explicit_sha256 is not None:
        expected_sha256.append(explicit_sha256.strip())
    sha_path = setup_path.parent / checksum_filename(version)
    if sha_path.exists():
        checksums = read_checksums(sha_path)
        if setup_path.name not in checksums:
            raise ValueError(f"Installer is missing from checksum file: {sha_path}")
        expected_sha256.append(checksums[setup_path.name])
    for expected in expected_sha256:
        if not re.fullmatch(r"[0-9a-fA-F]{64}", expected) or expected.upper() != actual_sha256:
            raise ValueError("Installer SHA256 does not match the supplied value or checksum file; regenerate after signing.")
    return actual_sha256


def release_url(version: str) -> str:
    return f"{REPO_URL}/releases/download/v{version}/{APP_NAME}-v{version}-{WINDOWS_X64_SUFFIX}-setup.msi"


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


def build_installer_manifest(version: str, installer_url: str, installer_sha256: str,
                             metadata: MsiMetadata) -> str:
    return schema_header("installer") + (
        f"PackageIdentifier: {PACKAGE_IDENTIFIER}\n"
        f"PackageVersion: {version}\n"
        f"InstallerType: msi\n"
        f"Dependencies:\n"
        f"  PackageDependencies:\n"
        f"  - PackageIdentifier: {VC_RUNTIME_PACKAGE}\n"
        f"Scope: user\n"
        f"ElevationRequirement: elevationProhibited\n"
        f"InstallModes:\n"
        f"- interactive\n"
        f"- silent\n"
        f"- silentWithProgress\n"
        f"UpgradeBehavior: install\n"
        f"ProductCode: {json.dumps(metadata.product_code)}\n"
        f"AppsAndFeaturesEntries:\n"
        f"- DisplayName: {DISPLAY_NAME}\n"
        f"  DisplayVersion: {version}\n"
        f"  Publisher: {PUBLISHER}\n"
        f"  ProductCode: {json.dumps(metadata.product_code)}\n"
        f"  UpgradeCode: {json.dumps(metadata.upgrade_code)}\n"
        f"  InstallerType: msi\n"
        f"Installers:\n"
        f"- Architecture: x64\n"
        f"  InstallerUrl: {installer_url}\n"
        f"  InstallerSha256: {installer_sha256}\n"
        f"  ProductCode: {json.dumps(metadata.product_code)}\n"
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
        f"ShortDescription: A Windows keyboard and mouse input automation tool.\n"
        f"Tags:\n"
        f"- input\n"
        f"- automation\n"
        f"- keyboard\n"
        f"- mouse\n"
        f"- windows\n"
        f"ReleaseNotesUrl: {release_notes_url}\n"
        f"ManifestType: defaultLocale\n"
        f"ManifestVersion: {MANIFEST_VERSION}\n"
    )


def main() -> None:
    args = parse_args()
    release_dir = Path(args.release_dir).resolve()
    output_dir = Path(args.output_dir).resolve()

    if args.installer is None and not release_dir.exists():
        raise FileNotFoundError(f"Release directory does not exist: {release_dir}")

    setup_path = args.installer.resolve() if args.installer else detect_setup_artifact(release_dir, args.version)
    metadata = read_msi_metadata(setup_path)
    version = extract_version(setup_path, args.version, metadata)
    installer_url = args.installer_url or release_url(version)
    release_notes_url = args.release_notes_url or notes_url(version)
    installer_sha256 = read_or_compute_sha256(setup_path, args.installer_sha256, version)

    target_dir = manifest_root(output_dir, version)
    target_dir.mkdir(parents=True, exist_ok=True)

    write_text(target_dir / f"{PACKAGE_IDENTIFIER}.yaml", build_version_manifest(version))
    write_text(target_dir / f"{PACKAGE_IDENTIFIER}.installer.yaml",
               build_installer_manifest(version, installer_url, installer_sha256, metadata))
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
