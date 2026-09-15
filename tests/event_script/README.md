# 事件剧本手动验收

这些文件是人工验收用例，不会被 CMake 自动执行。每次只把一个 `.fsevent` 文件复制到 Flori Input 的 `scripts`
目录，在事件剧本页刷新并选中它；选择文件时会自动校验。文本用例请先将焦点放到空白文本编辑器，
再按启动热键（默认 F8）；反向用例只需选中，不应启动。不要在游戏、终端或有未保存内容的窗口中运行。

| 文件                                            | 操作                            | 预期结果                                                                                             |
|-------------------------------------------------|---------------------------------|------------------------------------------------------------------------------------------------------|
| `valid/01_text_and_key.fsevent`                 | 选中后运行                      | 输入 `Flori Input 中文🙂`、一个 Tab、`OKA` 和换行，正常结束并回到就绪状态；`A` 由 Shift+A 组合键输入。 |
| `valid/02_nested_finite_repeat.fsevent`         | 选中后运行                      | 输入两行 `ABB`；覆盖嵌套有限循环、零按压/间隔与正等待。                                              |
| `valid/03_infinite_cancel.fsevent`              | 选中后运行，出现数行后再次按 F8 | 周期性输入 `tick`，第二次热键可停止，停止后不再输入。                                                |
| `valid/04_mouse_actions.fsevent`                | 修改坐标后选中运行              | 鼠标移到测试窗口、左击并向下滚动一步；点击后目标窗口仍须保持前台。                                   |
| `valid/05_event_hold_and_literal_text.fsevent` | 选中后运行                      | 输出大写 `A`，随后输出字面的 `\n` 和带双引号的 `quoted`；验证显式按下/抬起及纯文本。              |
| `invalid/01_wait_zero.fsevent`                  | 仅选中                          | 拒绝 `wait(0)`，不启动输入。                                                                         |
| `invalid/02_infinite_without_wait.fsevent`      | 仅选中                          | 拒绝没有时间推进的无限循环，不启动输入。                                                             |
| `invalid/03_wheel_zero.fsevent`                 | 仅选中                          | 拒绝零步滚轮动作。                                                                                   |
| `invalid/04_infinite_mouse_no_wait.fsevent`     | 仅选中                          | 拒绝仅含无等待鼠标动作的无限循环。                                                                   |
| `invalid/05_click_duration_overflow.fsevent`   | 仅选中                          | 超长按压时间须报告越界，而不是溢出后执行点击。                                                       |
| `invalid/06_move_outside_displays.fsevent`     | 仅选中                          | 报告坐标不在任何显示器上，不能把鼠标吸附到其他屏幕。                                                  |
| `runtime/01_duplicate_event_down.fsevent`     | 仅选中                      | 选择时拒绝重复按下，不注入 Shift。                                   |
| `runtime/02_unreleased_event.fsevent`         | 仅选中                      | 选择时拒绝未释放的输入，不注入 Shift。                                   |
| `limits/01_finite_step_over_hard_limit.fsevent` | 仅选中                          | 在任意合法配置下拒绝，报告有限剧本总步数超限。                                                       |
| `limits/02_no_wait_runtime_limit.fsevent`       | 仅选中                      | 选择时报告连续无等待步数超限，不启动。                                         |

补充静态验收用例：

| 文件 | 预期结果 |
| --- | --- |
| `invalid/07_event_up_without_down.fsevent` | 选择时拒绝无对应 Down 的 Up。 |
| `invalid/08_event_repeat_boundary.fsevent` | 选择时发现第二轮才出现的重复 Down。 |
| `invalid/09_stroke_held_target.fsevent` | 选择时拒绝对已按住目标执行 stroke。 |
| `limits/03_no_wait_across_blocks.fsevent` | 选择时发现跨两个循环累计的连续无等待步数超限。 |
| `valid/06_event_across_repeat.fsevent` | 校验通过；允许合法的跨循环按键配对，运行结束后 Shift 释放。 |

`runtime/` 及 `limits/02_no_wait_runtime_limit.fsevent` 保留原文件路径，预期已改为静态拒绝。
所有步数用例使用空文本避免注入字符；超限用例超过配置允许的硬上限。
静态校验按循环摘要计算，覆盖循环之间的连续无等待段，不展开大循环。
正按压时长、字符间隔和 wait 会重置连续无等待计数；空文本不会产生等待。
有限路径结束时必须释放全部显式输入；无限循环可保留外层按住的输入，由取消时清理。

窗口变化、显示器断开及系统注入失败仍须运行时处理，运行时保护和释放机制继续保留。

多屏幕验收时，把 `valid/04_mouse_actions.fsevent` 的 `move` 改成副屏内坐标（副屏位于主屏左侧或上方时应包含负坐标），确认鼠标确实到达该屏；有不同缩放比例的屏幕时也应检查。若显示器排列存在空白区域，再将坐标改为空白处，选择文件时应直接报错，不应启动。将剧本设为先 `wait(3000);` 再 `move`，在等待期间断开目标显示器，应在执行 `move` 时停止并报错。
