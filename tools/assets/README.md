# 应用图标生成

统一源为 `resources/icons/Flori-Input.svg`。`generate_icons.py` 使用项目 `.venv` 中的 resvg Python 绑定直接渲染目标尺寸，Pillow 将各尺寸封装为 ICO。开发依赖记录在 `pyproject.toml`，实际版本由 `uv.lock` 固定。脚本不调用全局 `resvg.exe`。

修改源 SVG 后，在项目根目录执行：

```powershell
uv sync --locked
uv run --locked python tools/assets/generate_icons.py
```

默认只更新 `resources/icons/Flori-Input.ico`，保留 16、20、24、32、40、48、64、96、128、256 像素 PNG 帧和透明度。每帧独立从 SVG 渲染，ICO 不放入 512 或 1024 像素帧。将生成的 ICO 与 SVG 修改一起提交；之后重新编译应用，确保 EXE 嵌入新图标。MSI 使用同一份 ICO。

如果实际用途需要一张通用 PNG，可单独输出一种分辨率：

```powershell
uv run --locked python tools/assets/generate_icons.py --png output/icons/Flori-Input.png --png-size 256
```

该命令只输出一张 PNG，不更新 ICO。也可用 `--ico resources/icons/Flori-Input.ico --png output/icons/Flori-Input.png` 同时生成。没有指定 `--png` 时，不生成通用 PNG，不保存中间尺寸文件。`--source-svg` 和 `--ico` 可指定其它输入或输出路径。

MSIX 打包入口通过同一个 Python 解释器调用 `--msix-assets`，从 SVG 直接生成清单要求的 PNG 和任务栏尺寸变体，全部写入打包暂存目录。MSIX 素材不读取 ICO，也不在仓库保存多尺寸 PNG。详见 [MSIX 制作与验证](../packaging/msix/README.md)。

WinGet 目录展示图标仍由目录元数据和展示端选择决定。生成通用 PNG 不会自动改变已经发布的目录图标；其接入需另行确认。
