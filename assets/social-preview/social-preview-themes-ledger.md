# Flori Input themed social preview design ledger

```yaml
repository:
  name: "Flori Input"
  url: "https://github.com/flowersauce/Flori-Input"
  visibility: public
  local_checkout: "D:/Users/Flowersauce/Documents/Github_Projects/Flori-Input"

evidence:
  mode: local-static
  inspected:
    - path_or_url: "README.md"
      proves: "项目定位与三类核心能力"
    - path_or_url: "CHANGELOG.md"
      proves: "Slint UI 重构及相较旧架构降低内存使用"
    - path_or_url: "resources/fonts/FloriInputUI-Medium.ttf"
      proves: "本地字体内部家族名 FloriInputUI、Medium 样式，并覆盖封面所需全部字符"
    - path_or_url: "docs/legal/fonts.md"
      proves: "字体由 Jura 英文和符号与 Sarasa UI SC 中文字形组合"
    - path_or_url: "resources/icons/Flori-Input_transparent.svg"
      proves: "透明底花形与涟漪品牌标识"
    - path_or_url: "resources/themes/dark.jsonc"
      proves: "深色主题色板"
    - path_or_url: "resources/themes/light.jsonc"
      proves: "浅色主题色板"
    - path_or_url: "user-provided product information"
      proves: "运行时内存占用仅约 20 MiB"
  claim_boundaries:
    - "未执行构建、运行、测试或内存测量。"
    - "约 20 MiB 为用户提供的近似产品数据。"
    - "PNG 文字由 Pillow 直接加载仓库 TTF 绘制，不使用系统字体回退。"

content:
  promise: "面向 Windows 的开源键鼠自动化工具"
  proof:
    - "高速按键输入"
    - "文本模拟输入"
    - "自动化事件编排"
  exclude:
    - "软件界面快照"
    - "构建工具链与依赖版本"

source_route:
  artifacts:
    - path_or_url: "resources/icons/Flori-Input_transparent.svg"
      role: brand
      identity_value: 3
      intrinsic_beauty: 3
      composition_readiness: 3
      noise_burden: 0
      information_deficit: 2
    - path_or_url: "resources/fonts/FloriInputUI-Medium.ttf"
      role: component
      identity_value: 3
      intrinsic_beauty: 3
      composition_readiness: 3
      noise_burden: 0
      information_deficit: 0
  intervention:
    keep:
      - "透明花形与同心涟漪"
      - "左右分区、文案和仓库地址"
    remove:
      - "渲染阶段的系统字体回退可能性"
    repair:
      - "PNG 直接从本地 TTF 绘制全部文本"
      - "提供仓库原生深色与浅色主题版本"
      - "按 SVG 组透明语义合成内部花蕊，消除逐圆透明度叠加"
      - "深浅主题 Logo 均恢复原始品牌色；GitHub 圆点改用 Atom 紫色"
    supplement: []
  interpretation_level: 1
  continuity_model: isolated-artifact
  aspect_fit:
    source_bounds: "1024x1024 transparent SVG"
    target_bounds: "约 480x480，位于画布右侧"
    method: contain
    unused_area: "左侧空间专用于名称、说明、能力与仓库地址"
    verdict: pass
  fragment_ledger:
    - source_region: "完整透明品牌标识"
      semantic_unit: "花形与全部同心涟漪"
      crop_boundary: "无裁切"
      adjacency: "与左侧文案明确分区"
      thumbnail_verdict: pass

composition:
  production_route: code-native-svg
  regions:
    - name: "名称与价值主张"
      bounds: "80,128 to 670,360"
      purpose: "说明项目身份、开源属性、Slint UI 与内存特点"
    - name: "功能证明"
      bounds: "80,394 to 652,448"
      purpose: "展示三项核心能力"
    - name: "仓库地址"
      bounds: "80,514 to 650,562"
      purpose: "提供项目来源"
    - name: "品牌主视觉"
      bounds: "742,78 to 1222,558"
      purpose: "展示透明底原始标识"
  line_ledger:
    - element: "同心蓝色涟漪"
      role: "inherited source geometry"
      endpoints_or_bounds: "围绕花形中心形成完整闭合圆"
      evidence: "resources/icons/Flori-Input_transparent.svg"

version:
  baseline: "assets/social-preview.svg and assets/social-preview.png"
  candidate: "assets/social-preview-dark.* and assets/social-preview-light.*"
  preservation_contract:
    identity_anchors:
      - "Flori Input 名称"
      - "透明花形与涟漪标识"
      - "仓库组合字体"
      - "仓库地址"
    protected_strengths:
      - "清晰的左右分区"
      - "缩略图下可辨识的品牌与项目名"
    allowed_changes:
      - "使用直接 TTF 渲染"
      - "增加浅色主题"
      - "修正 Logo 组透明合成"
      - "更新 GitHub 链接圆点颜色"
    forbidden_changes:
      - "系统字体回退"
      - "带底板 Logo"
      - "软件快照"
  comparison_scores:
    identity_fidelity: 3
    product_clarity: 2
    aesthetic_authorship: 2
    abstraction_fit: 3
    topology_fidelity: 0
    material_quality: 3
    composition: 3
    thumbnail_legibility: 3
    line_semantics: 3
    fragment_integrity: 3
  vetoes: []
  verdict: promote
  reason: "修正内部花瓣的组透明合成并恢复原始 Logo 配色，同时按主题应用 Atom 紫色链接标记。"

output:
  svg:
    - "assets/social-preview/social-preview-dark.svg"
    - "assets/social-preview/social-preview-light.svg"
  png:
    - "assets/social-preview/social-preview-dark.png"
    - "assets/social-preview/social-preview-light.png"
  bilibili_png: "assets/social-preview/social-preview-dark-bilibili.png"
  review_sheet: "assets/social-preview/social-preview-themes-review.png"
  width: 1280
  height: 640
  bytes:
    dark_png: 140380
    light_png: 126454
  mechanical_validation: pass
  full_size_review: pass
  thumbnail_light_review: pass
  thumbnail_dark_review: pass
  batch_contact_review: not-applicable

authorization:
  readme_modified: false
  github_uploaded: false
  upload_verification: not-requested
```
