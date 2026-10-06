# Microsoft Store 素材清单

本目录集中保存商店展示图片；截图对应 Flori Input 1.5.0，采集于 2026-10-06。
安装包位于 `output/store/`，展示素材不随应用打包。

## 目录

```text
microsoft-store/
├── README.md
├── app-icon.png            # 中英文共用的透明 Logo
├── zh-CN/
│   ├── cover.png
│   ├── trigger-dark.png
│   └── paste-light.png
└── en-US/
    ├── cover.svg           # 英文头图源文件，不上传
    ├── cover.png
    ├── trigger-dark.png
    └── paste-light.png
```

## 上传顺序

所有截图和头图均为 **1600×900、16:9、PNG**。下列头图也上传到“屏幕截图 → 桌面”。

| 页面语言 | 顺序 | 文件 | 截图说明（可直接填写） |
| --- | --- | --- | --- |
| 中文（中国） | 1 | [zh-CN/cover.png](zh-CN/cover.png) | Flori Input：面向 Windows 的开源键鼠自动化工具。 |
| 中文（中国） | 2 | [zh-CN/trigger-dark.png](zh-CN/trigger-dark.png) | 深色模式下配置热键、输入行为和连点间隔。 |
| 中文（中国） | 3 | [zh-CN/paste-light.png](zh-CN/paste-light.png) | 浅色模式下配置模拟按键、Unicode 文本输入和热键。 |
| English (United States) | 1 | [en-US/cover.png](en-US/cover.png) | Flori Input: open-source input automation for Windows. |
| English (United States) | 2 | [en-US/trigger-dark.png](en-US/trigger-dark.png) | Configure hotkeys, input actions, and repeat timing in dark mode. |
| English (United States) | 3 | [en-US/paste-light.png](en-US/paste-light.png) | Use light mode to configure text input timing and hotkeys. |

两个语言页面的“Microsoft Store 徽标 → 1:1 应用磁贴图标（300×300）”均使用
[app-icon.png](app-icon.png)。其他徽标尺寸、独立的“16:9 超级主图”、Xbox 图像和预告片可留空。
Logo 为 **300×300、RGBA PNG**，保留花形与涟漪，外围透明。

## 制作与更新

拍摄前先打开应用，然后在项目根目录执行 `.\tools\assets\Center-FloriInput.ps1` 将窗口居中。
多个版本同时打开时使用 `-ProcessId <进程 ID>` 选择目标。

- 中文头图源为 [现有深色封面](../social-preview/social-preview-dark.png)，可编辑源为
  [同名 SVG](../social-preview/social-preview-dark.svg)。保留原版文字、字体、标识和左右构图，
  将 1280×640 按原比例放大到 1600×800，上下各延展 50 像素同色背景。
- 英文头图源为 [en-US/cover.svg](en-US/cover.svg)，沿用原封面的布局、配色和标识，翻译文案，
  使用仓库中的 `FloriInputUI-Medium.ttf` 导出。
- 四张原始截图名为 `dark-CN.png`、`light-CN.png`、`dark-EN.png`、`light-EN.png`。
  当前原图均为 3840×2160；裁剪范围为 `(1120, 630) → (2720, 1530)`，保留完整居中窗口。
  中文和英文深色图为“触发器”，浅色图为“模拟粘贴”。
  截图不缩放、不重绘、不添加文字，原始界面像素保持一致。
- Logo 从 `resources/icons/Flori-Input_transparent.svg` 直接渲染为 300×300。
- `assets/social-preview/` 和截图输入目录均只读；本目录保存最终上传图片，不保留原始桌面截图副本。

只重新生成中英文头图和 Logo：

```powershell
uv run --locked python tools/assets/generate_store_images.py
```

同时处理四张原始截图（原始窗口需居中）：

```powershell
uv run --locked python tools/assets/generate_store_images.py --screenshots-dir '你的截图输入目录'
```

## 检查记录

保留项目名、字体、原始花形与涟漪、功能标签和仓库地址；调整头图比例，补充英文文案，并裁剪截图外围桌面。
原封面继续作为 GitHub 社交预览使用，商店版本独立保存。图片尺寸、截图像素一致性及完整窗口需在更新后复核。
本次已检查全部图片尺寸和完整画面；四张截图与原图对应区域的 RGB 像素完全一致，头图通过完整尺寸及缩略图对照。

规范参考：[Microsoft Store 截图与图像要求](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/screenshots-and-images)。
