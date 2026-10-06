#!/usr/bin/env python3
"""适配已有项目封面，裁剪真实截图，并导出 Microsoft Store 展示素材。"""

from __future__ import annotations

import argparse
from io import BytesIO
from pathlib import Path

from PIL import Image

from generate_icons import ROOT, SOURCE_SVG, SvgIcon, write_asset

STORE_DIRECTORY = ROOT / "assets" / "microsoft-store"
SCREENSHOT_SIZE = (1600, 900)
COVER_SOURCE = ROOT / "assets" / "social-preview" / "social-preview-dark.png"
ENGLISH_COVER_SOURCE = STORE_DIRECTORY / "en-US" / "cover.svg"
TRANSPARENT_ICON_SOURCE = SOURCE_SVG.with_name("Flori-Input_transparent.svg")
SCREENSHOTS = {
    "dark-CN.png": "zh-CN/trigger-dark.png",
    "light-CN.png": "zh-CN/paste-light.png",
    "dark-EN.png": "en-US/trigger-dark.png",
    "light-EN.png": "en-US/paste-light.png",
}


def save_png(image: Image.Image, name: str) -> None:
    """无损保存到商店素材目录，不向截图来源目录写入文件。"""
    destination = STORE_DIRECTORY / name
    with BytesIO() as output:
        image.save(output, format="PNG", optimize=True)
        write_asset(destination, output.getvalue())
    print(f"PNG（{image.width}×{image.height}）：{destination}")


def create_cover() -> None:
    """保留原封面的完整构图，用同色背景补足 16:9 画布。"""
    with Image.open(COVER_SOURCE) as source:
        if source.width != source.height * 2:
            raise ValueError("项目封面源图应保持 2:1 比例。")
        width, height = SCREENSHOT_SIZE
        with source.convert("RGB") as original:
            with original.resize((width, width // 2), Image.Resampling.LANCZOS) as artwork:
                with Image.new("RGB", SCREENSHOT_SIZE, original.getpixel((0, 0))) as cover:
                    cover.paste(artwork, (0, (height - artwork.height) // 2))
                    save_png(cover, "zh-CN/cover.png")


def create_english_cover() -> None:
    """从英文 SVG 直接导出，使用仓库字体保持原封面的视觉风格。"""
    import resvg

    options = resvg.usvg.Options.default()
    options.resources_dir = str(ENGLISH_COVER_SOURCE.parent)
    options.load_font_file(str(ROOT / "resources" / "fonts" / "FloriInputUI-Medium.ttf"))
    tree = resvg.usvg.Tree.from_str(ENGLISH_COVER_SOURCE.read_text(encoding="utf-8"), options)
    if tuple(tree.int_size()) != SCREENSHOT_SIZE:
        raise ValueError("英文头图 SVG 尺寸应为 1600×900。")
    data = bytes(resvg.render(tree, (1.0, 0.0, 0.0, 0.0, 1.0, 0.0), bg_size=SCREENSHOT_SIZE))
    destination = ENGLISH_COVER_SOURCE.with_suffix(".png")
    write_asset(destination, data)
    print(f"PNG（1600×900）：{destination}")


def crop_screenshots(directory: Path) -> None:
    """裁剪居中窗口周围的桌面，不缩放、重绘或修改软件界面。"""
    width, height = SCREENSHOT_SIZE
    for source_name, output_name in SCREENSHOTS.items():
        with Image.open(directory / source_name) as source:
            if source.width < width or source.height < height:
                raise ValueError(f"原始截图小于目标尺寸：{source_name}（{source.size}）。")
            left = (source.width - width) // 2
            top = (source.height - height) // 2
            with source.crop((left, top, left + width, top + height)) as cropped:
                with cropped.convert("RGB") as image:
                    save_png(image, output_name)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--screenshots-dir", type=Path,
        help="包含四张窗口居中的原始 PNG 的只读输入目录；省略时保留现有截图。",
    )
    args = parser.parse_args()

    try:
        icon = SvgIcon(TRANSPARENT_ICON_SOURCE)
        icon_output = STORE_DIRECTORY / "app-icon.png"
        write_asset(icon_output, icon.png(300))
        print(f"Store Logo（300×300）：{icon_output}")
        create_cover()
        create_english_cover()
        if args.screenshots_dir is not None:
            crop_screenshots(args.screenshots_dir.resolve(strict=True))
    except ModuleNotFoundError as error:
        if error.name != "resvg":
            raise
        raise SystemExit("缺少 SVG 渲染依赖。请先执行 uv sync，再使用 uv run python 运行此命令。") from error


if __name__ == "__main__":
    main()
