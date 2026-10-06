"""WiX MSI 制作与实际 Windows Installer 元数据读取；仅使用 Python 标准库。"""

from __future__ import annotations

import ctypes
import os
import re
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

WIX_VERSION = "7.0.0"
WIX_NAMESPACE = "http://wixtoolset.org/schemas/v4/wxs"
COMPONENT_NAMESPACE = uuid.UUID("4bf858ce-49aa-4bf7-835c-02a2bf22e706")


@dataclass(frozen=True)
class MsiMetadata:
    """从 MSI Property 表读取的发布身份。"""

    version: str
    product_code: str
    upgrade_code: str
    name: str
    manufacturer: str


def validate_msi_version(version: str) -> None:
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError(f"MSI 需要三段数字版本号：{version!r}")
    major, minor, patch = map(int, version.split("."))
    if major > 255 or minor > 255 or patch > 65535:
        raise ValueError(f"MSI 版本号超出 Windows Installer 范围：{version}")


def product_code(version: str) -> str:
    """同一正式版本复用产品身份；更高版本使用新的 ProductCode。"""
    validate_msi_version(version)
    return "{" + str(uuid.uuid5(COMPONENT_NAMESPACE, f"product:x64:user:{version}")).upper() + "}"


def write_payload(application_dir: Path, destination: Path) -> None:
    """每个文件使用稳定组件 GUID 和 HKCU key path，适配按用户安装。"""
    ET.register_namespace("", WIX_NAMESPACE)
    wix = ET.Element(f"{{{WIX_NAMESPACE}}}Wix")
    fragment = ET.SubElement(wix, "Fragment")
    group = ET.SubElement(fragment, "ComponentGroup", Id="ApplicationFiles", Directory="INSTALLFOLDER")
    for file in sorted(application_dir.iterdir()):
        if not file.is_file() or file.name == "portable.flag":
            raise ValueError(f"MSI 内容目录只能包含应用文件，不能含便携标记或子目录：{file}")
        identity = uuid.uuid5(COMPONENT_NAMESPACE, f"file:{file.name.casefold()}")
        component_id = f"FileComponent_{identity.hex}"
        component = ET.SubElement(group, "Component", Id=component_id, Guid="{" + str(identity).upper() + "}")
        file_id = "ApplicationExecutable" if file.name == "Flori-Input.exe" else f"File_{identity.hex}"
        ET.SubElement(component, "File", Id=file_id, Source=str(file), Name=file.name)
        ET.SubElement(component, "RegistryValue", Root="HKCU",
                      Key=r"Software\Flowersauce\Flori-Input\Installer\Components",
                      Name=component_id, Type="integer", Value="1", KeyPath="yes")
    ET.indent(wix)
    ET.ElementTree(wix).write(destination, encoding="utf-8", xml_declaration=True)


def write_license(license_file: Path, destination: Path) -> None:
    notice = (
        "Uninstalling Flori Input permanently deletes its private configuration, scripts, logs and preview data. "
        "Copy files you want to keep outside %LOCALAPPDATA%\\Flori-Input before uninstalling. "
        "Upgrades and repairs preserve this data.\n\n"
    )
    content = notice + license_file.read_text(encoding="utf-8")
    escaped = content.replace("\\", r"\\").replace("{", r"\{").replace("}", r"\}")
    destination.write_text(r"{\rtf1\ansi\deff0{\fonttbl{\f0 Segoe UI;}}\f0\fs20 "
                           + escaped.replace("\n", "\\par\n") + "}", encoding="ascii")


def create_msi(application_dir: Path, destination: Path, version: str, wix_command: str, root: Path) -> None:
    validate_msi_version(version)
    wix_path = shutil.which(wix_command)
    if wix_path is None:
        raise FileNotFoundError(f"找不到 WiX：{wix_command}；请按 tools/packaging/windows/README.md 准备工具。")
    actual_version = subprocess.run([wix_path, "--version"], cwd=root, check=True,
                                    capture_output=True, text=True).stdout.strip()
    if actual_version.split("+", 1)[0] != WIX_VERSION:
        raise ValueError(f"需要 WiX {WIX_VERSION}，当前工具版本为 {actual_version!r}。")
    work_dir = destination.parent / "msi-source"
    work_dir.mkdir()
    payload = work_dir / "ApplicationFiles.wxs"
    license_rtf = work_dir / "License.rtf"
    write_payload(application_dir, payload)
    write_license(root / "LICENSE", license_rtf)
    command = [
        wix_path, "build", "-arch", "x64", "-culture", "en-us",
        "-ext", f"WixToolset.UI.wixext/{WIX_VERSION}",
        "-ext", f"WixToolset.Util.wixext/{WIX_VERSION}",
        "-d", f"AppVersion={version}", "-d", f"ProductCode={product_code(version)}",
        "-d", f"AppIcon={root / 'resources' / 'icons' / 'Flori-Input.ico'}",
        "-d", f"LicenseRtf={license_rtf}", "-pdbtype", "none",
        "-intermediateFolder", str(work_dir / "obj"), "-out", str(destination),
        str(root / "tools" / "packaging" / "windows" / "FloriInput.wxs"), str(payload),
    ]
    subprocess.run(command, cwd=root, check=True)
    metadata = read_msi_metadata(destination)
    expected_upgrade_code = "{" + str(COMPONENT_NAMESPACE).upper() + "}"
    if (metadata.version != version or metadata.product_code != product_code(version)
            or metadata.upgrade_code != expected_upgrade_code):
        raise ValueError("生成的 MSI 身份与当前构建版本不一致。")


def read_msi_metadata(path: Path) -> MsiMetadata:
    """以只读方式调用 msi.dll；不安装或执行 MSI。"""
    if os.name != "nt":
        raise OSError("读取 MSI 元数据需要 Windows。")
    msi = ctypes.WinDLL("msi", use_last_error=True)
    handle_type = ctypes.c_uint
    pointer = ctypes.POINTER(handle_type)
    signatures = {
        "MsiOpenDatabaseW": ([ctypes.c_wchar_p, ctypes.c_void_p, pointer], ctypes.c_uint),
        "MsiDatabaseOpenViewW": ([handle_type, ctypes.c_wchar_p, pointer], ctypes.c_uint),
        "MsiViewExecute": ([handle_type, handle_type], ctypes.c_uint),
        "MsiViewFetch": ([handle_type, pointer], ctypes.c_uint),
        "MsiCreateRecord": ([ctypes.c_uint], handle_type),
        "MsiRecordSetStringW": ([handle_type, ctypes.c_uint, ctypes.c_wchar_p], ctypes.c_uint),
        "MsiRecordGetStringW": ([handle_type, ctypes.c_uint, ctypes.c_wchar_p, pointer], ctypes.c_uint),
        "MsiCloseHandle": ([handle_type], ctypes.c_uint),
    }
    for name, (arguments, result) in signatures.items():
        function = getattr(msi, name)
        function.argtypes = arguments
        function.restype = result

    def check(result: int, operation: str) -> None:
        if result != 0:
            raise OSError(result, f"{operation} 失败：{path}")

    @contextmanager
    def owned_handle(value: int) -> Iterator[int]:
        if not value:
            raise OSError(f"Windows Installer 无法创建句柄：{path}")
        try:
            yield value
        finally:
            msi.MsiCloseHandle(value)

    database = handle_type()
    check(msi.MsiOpenDatabaseW(str(path.resolve()), None, ctypes.byref(database)), "打开 MSI")
    with owned_handle(database.value) as database_handle:
        def read_property(name: str) -> str:
            view = handle_type()
            check(msi.MsiDatabaseOpenViewW(database_handle,
                                         "SELECT `Value` FROM `Property` WHERE `Property` = ?",
                                         ctypes.byref(view)), "查询 MSI Property 表")
            with owned_handle(view.value) as view_handle, owned_handle(msi.MsiCreateRecord(1)) as parameters:
                check(msi.MsiRecordSetStringW(parameters, 1, name), "设置查询参数")
                check(msi.MsiViewExecute(view_handle, parameters), "执行查询")
                record = handle_type()
                result = msi.MsiViewFetch(view_handle, ctypes.byref(record))
                if result == 259:  # ERROR_NO_MORE_ITEMS
                    return ""
                check(result, "读取查询结果")
                with owned_handle(record.value) as record_handle:
                    length = handle_type()
                    result = msi.MsiRecordGetStringW(record_handle, 1, None, ctypes.byref(length))
                    if result not in (0, 234):  # ERROR_MORE_DATA
                        check(result, "读取属性长度")
                    buffer = ctypes.create_unicode_buffer(length.value + 1)
                    length.value += 1
                    check(msi.MsiRecordGetStringW(record_handle, 1, buffer, ctypes.byref(length)), "读取属性")
                    return buffer.value

        metadata = MsiMetadata(read_property("ProductVersion"), read_property("ProductCode"),
                               read_property("UpgradeCode"), read_property("ProductName"),
                               read_property("Manufacturer"))
        if read_property("ALLUSERS"):
            raise ValueError("发布 MSI 必须是固定的按用户安装包。")
    validate_msi_version(metadata.version)
    for value in (metadata.product_code, metadata.upgrade_code):
        if not re.fullmatch(r"\{[0-9A-Fa-f-]{36}\}", value):
            raise ValueError(f"MSI GUID 无效：{value!r}")
        uuid.UUID(value)
    if metadata.name != "Flori Input" or metadata.manufacturer != "Flowersauce":
        raise ValueError("MSI 不是 Flowersauce 发布的 Flori Input。")
    return metadata
