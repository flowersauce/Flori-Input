#!/usr/bin/env python3
"""从统一 SVG 源生成应用 ICO、可选单张 PNG 和 MSIX 暂存图标。"""

from __future__ import annotations

import argparse
import tempfile
from contextlib import ExitStack
from io import BytesIO
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE_SVG = ROOT / "resources" / "icons" / "Flori-Input.svg"
APPLICATION_ICO = SOURCE_SVG.with_suffix(".ico")
ICO_SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
MSIX_ASSETS = {
    "StoreLogo.png": 50,
    "Square44x44Logo.png": 44,
    "Square150x150Logo.png": 150,
    "Square44x44Logo.targetsize-16_altform-unplated.png": 16,
    "Square44x44Logo.targetsize-24_altform-unplated.png": 24,
    "Square44x44Logo.targetsize-32_altform-unplated.png": 32,
    "Square44x44Logo.targetsize-48_altform-unplated.png": 48,
    "Square44x44Logo.targetsize-256_altform-unplated.png": 256,
}


class SvgIcon:
    """读取方形 SVG，并直接渲染各目标尺寸，保持透明背景。"""

    def __init__(self, source: Path) -> None:
        import resvg

        source = source.resolve(strict=True)
        options = resvg.usvg.Options.default()
        options.resources_dir = str(source.parent)
        self.tree = resvg.usvg.Tree.from_str(source.read_text(encoding="utf-8"), options)
        width, height = self.tree.int_size()
        if width != height or width <= 0:
            raise ValueError("应用 SVG 必须具有正数且相等的宽高。")
        self.source_size = width
        self.png_frames: dict[int, bytes] = {}

    def png(self, size: int) -> bytes:
        """每个尺寸均从矢量源渲染，缓存同尺寸的无损 PNG。"""
        import resvg

        if size not in self.png_frames:
            scale = size / self.source_size
            self.png_frames[size] = bytes(resvg.render(
                self.tree, (scale, 0.0, 0.0, 0.0, scale, 0.0), bg_size=(size, size),
            ))
        return self.png_frames[size]


def write_asset(destination: Path, data: bytes) -> None:
    """先写入同目录暂存文件，再替换产物，保留生成失败前的旧文件。"""
    destination = destination.resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(dir=destination.parent, suffix=".tmp", delete=False) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(data)
        temporary_path.replace(destination)
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def create_ico(icon: SvgIcon, destination: Path) -> None:
    """将各尺寸的独立 SVG 渲染结果交给 Pillow 封装为多帧 ICO。"""
    from PIL import Image

    with ExitStack() as stack:
        frames = []
        for size in ICO_SIZES:
            with BytesIO(icon.png(size)) as source:
                with Image.open(source) as image:
                    if image.size != (size, size):
                        raise ValueError(f"SVG 渲染尺寸错误：期望 {size}，实际 {image.size}。")
                    frame = image.convert("RGBA")
            stack.callback(frame.close)
            frames.append(frame)
        with BytesIO() as output:
            frames[-1].save(
                output, format="ICO", sizes=[(size, size) for size in ICO_SIZES],
                append_images=frames[:-1],
            )
            write_asset(destination, output.getvalue())


def png_size(value: str) -> int:
    """限制单张 PNG 的尺寸，避免误填造成过大的渲染分配。"""
    size = int(value)
    if not 1 <= size <= 4096:
        raise argparse.ArgumentTypeError("PNG 尺寸必须介于 1 和 4096 像素之间。")
    return size


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-svg", type=Path, default=SOURCE_SVG, help="统一的方形 SVG 源。")
    parser.add_argument("--ico", type=Path, help="ICO 输出路径；没有指定任何输出时生成应用 ICO。")
    parser.add_argument("--png", type=Path, help="可选的单张通用 PNG 输出路径。")
    parser.add_argument("--png-size", type=png_size, default=256, help="单张 PNG 的边长，默认 256。")
    parser.add_argument("--msix-assets", type=Path, help="仅供 MSIX 打包的 Assets 暂存目录。")
    args = parser.parse_args()
    if args.ico is None and args.png is None and args.msix_assets is None:
        args.ico = APPLICATION_ICO
    for path, suffix in ((args.ico, ".ico"), (args.png, ".png")):
        if path is not None and path.suffix.casefold() != suffix:
            parser.error(f"输出文件必须使用 {suffix} 扩展名：{path}")

    try:
        icon = SvgIcon(args.source_svg)
        if args.ico is not None:
            create_ico(icon, args.ico)
            print(f"ICO：{args.ico.resolve()}")
        if args.png is not None:
            write_asset(args.png, icon.png(args.png_size))
            print(f"PNG（{args.png_size}×{args.png_size}）：{args.png.resolve()}")
        if args.msix_assets is not None:
            for name, size in MSIX_ASSETS.items():
                write_asset(args.msix_assets / name, icon.png(size))
            print(f"MSIX 图标：{args.msix_assets.resolve()}")
    except ModuleNotFoundError as error:
        if error.name not in {"resvg", "PIL"}:
            raise
        raise SystemExit("缺少图标生成依赖。请先执行 uv sync，再使用 uv run python 运行此命令。") from error


if __name__ == "__main__":
    main()
