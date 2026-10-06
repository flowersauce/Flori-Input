# 第三方许可原文

这些文件供发布打包使用。`tools/release/package_app.py` 将下列原文和组件来源合并为
`THIRD-PARTY-NOTICES.txt`，与程序一起放入 ZIP、MSI 和 MSIX，打包时不访问网络。
Flori Input 本体的 MIT 许可仍独立保留为 `LICENSE`。

收集日期：2026-10-06。

| 文件 | 来源 |
| --- | --- |
| [Slint-Royalty-free-2.0.md](Slint-Royalty-free-2.0.md) | [Slint v1.17.1 的许可原文](https://github.com/slint-ui/slint/blob/v1.17.1/LICENSES/LicenseRef-Slint-Royalty-free-2.0.md) |
| [Slint-1.17.1-THIRDPARTY.md](Slint-1.17.1-THIRDPARTY.md) | 当前使用的官方 Slint 1.17.1 C++ SDK 的 `licenses/THIRDPARTY.md`，原样复制；[发行页面](https://github.com/slint-ui/slint/releases/tag/v1.17.1) |
| [Glaze-8.3.0-LICENSE.txt](Glaze-8.3.0-LICENSE.txt) | 当前 Glaze 8.3.0 的 vcpkg `share/glaze/copyright`，原样复制；[上游许可](https://github.com/stephenberry/glaze/blob/v8.3.0/LICENSE) |
| [Jura-OFL.txt](Jura-OFL.txt) | [Google Fonts Jura 的 OFL 原文](https://github.com/google/fonts/blob/main/ofl/jura/OFL.txt)，版权声明与内置字体元数据一致 |
| [Sarasa-Gothic-LICENSE.txt](Sarasa-Gothic-LICENSE.txt) | [Sarasa Gothic 上游许可与版权声明](https://github.com/be5invis/Sarasa-Gothic/blob/main/LICENSE)，包含 Inter、Adobe 和 Google 的部分字形声明 |
| [CC-BY-ND-4.0.txt](CC-BY-ND-4.0.txt) | [Creative Commons 官方许可原文](https://creativecommons.org/licenses/by-nd/4.0/legalcode.txt)；Slint 标识的作者与许可依据为 [Slint v1.17.1 REUSE 元数据](https://github.com/slint-ui/slint/blob/v1.17.1/REUSE.toml) |

`FloriInputUI-Medium.ttf` 是由 Jura 和 Sarasa UI SC 字形组合的派生字体，名称已改为
FloriInputUI，仍适用 SIL OFL 1.1。这里保留原文，不翻译或改写许可条件。

Slint 的依赖清单按 SDK 原样保留，其中可能包括不同平台或可选功能的组件。
升级 Slint、Glaze 或替换字体/素材时，同步更新对应原文、来源和打包脚本中的版本说明。
