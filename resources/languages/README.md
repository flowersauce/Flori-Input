# 内置翻译资源

`zh-CN.jsonc` 和 `en-US.jsonc` 由 CMake 嵌入程序，修改后需要重新构建。

- `strings`：界面语义键，采用 `功能域.语义名称`，小写单词用下划线连接。
  例如 `nav.settings`、`paste.clipboard_hint`、`error.hotkey_hook_failed`。
  通用操作使用 `common`，状态提示使用 `status`；不要用整句文案作为键名。
  两种语言保持相同键集及排列顺序，修改键名时同步 C++ 和 Slint 引用。
- `diagnostics`：异常和控制器状态的语义键，采用同样的 `功能域.语义名称` 格式。
  例如 `script.read_failed`、`clipboard.busy`、`execution.step_limit`。
- `keys`：按键名称，统一采用 `key.*`，例如 `key.mouse_left`、`key.enter`、`key.f8`。
  `key_catalog.cpp` 将虚拟键码映射到这些键；配置仍保存原有数字键码。

所有键只使用小写 ASCII 字母、数字、下划线和分类分隔点，不包含翻译原文、空格或标点句子。
每个分类按键名排序，两种语言使用相同顺序。

## 诊断分类

| 前缀 | 内容 |
| --- | --- |
| `script` | 剧本解析、文件读取和编辑 |
| `program` | 输入程序结构和参数校验 |
| `execution` | 执行会话、步数限制和输入清理 |
| `input` | 原生输入后端与 Windows 错误 |
| `clipboard` / `paste` | 剪贴板与模拟粘贴 |
| `trigger` | 触发器状态与配置 |
| `encoding` | UTF-8 / UTF-16 校验 |
| `config` / `runtime` | 配置及运行时依赖 |

## 诊断传递

异常和状态字符串通过 `[[功能域.语义键]]` 标记引用翻译：

```cpp
throw InputError("[[clipboard.read_failed]]");
```

`UiResources::message()` 只替换显式标记，不再匹配中英原文；标记外的行列号、坐标和错误码原样保留。
例如 `12:3 [[script.read_failed]]` 显示为 `12:3 剧本读取失败`。
动态错误的前缀、后缀译文应保留必要的标点与空格。译文不递归展开。
第三方异常原文仍可透传；应用内的诊断应使用语义标记，新增时同步两种语言。

手动静态检查（无第三方依赖，不构建或运行应用）：

```powershell
python tools/check_translations.py
```

检查所有分类的键名规范、重复键、语言键集、界面引用、诊断标记、闲置文本及按键目录一致性。
同时拒绝中英混合诊断原文，以及 `fail()` 和常用输入异常中的未标记字面量。
这不是 C++/Slint 编译器；变量转发与第三方异常仍需人工核对。

本地验证时重点检查：语言切换后的状态和错误、剧本解析行列号、读取失败、屏幕外坐标、
SendInput 错误码及按键标签。核心异常的 `what()` 现在包含语义标记，展示给用户前须经过 `message()`。
