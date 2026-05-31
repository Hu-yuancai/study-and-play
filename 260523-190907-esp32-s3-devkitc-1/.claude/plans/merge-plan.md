# 合并方案：扫雷 + Ghost Shell + 按键队列 + 新配色

## 目标
把 `plus/minesweeper/` 和 `260523-...(3)` 两个新文件夹里的改进，合并进当前能跑的版本。当前版已修复的内容（钢琴判定、非阻塞音频、中文路径编译、串口配置）**必须保留，不被覆盖**。

## 关键决策：扫雷是"新增第4项"，不是替换飞机大战
plus 版用扫雷换掉了飞机大战。但用户要**两个都保留**。所以主菜单从 3 项扩展为 **4 项**：钢琴 / 飞机大战 / 扫雷 / 学习伴侣。这是和 plus 版最大的结构差异，菜单循环取模要从 `%3` 改成 `%4`。

## 兼容性核对结论（已全部验证）
- 新模块用的 OLED 函数（oledShowFace/oledShowBlinkingFace/oledDrawBorderText/oledClear/oledDrawString/oledRefresh/oledShowWelcome）和 FACE 宏（含 FACE_PIANO_5）当前版**全部已有**，编号一致 → **不需要替换 OLED 驱动**，保留当前位图表情。
- 新模块用的 LCD 函数、音频函数（playNote/playSFX_*/playTone/stopTone）当前版**全部已有**。
- `KEY_QUEUE_SIZE` 定义在 keyboard.h 内（自包含），队列 API 自带。
- minesweeper 用 `Preferences.h`（ESP32 内置，存档到 Flash），**无需新增库依赖**。
- ghost_shell 退出靠 `KEY_BACK`（物理 `*` 键，可达）。`KEY_9` 只是冗余入口，按不出来不影响。
- plus 版 main.cpp 的中文字符串是正确 UTF-8（OLED 用 wqy GB2312 字体）→ 合并时统一保证 UTF-8，无乱码。

## 需要新增的文件（从 plus/minesweeper 复制，原样保留中文注释）
1. `src/minesweeper.cpp` + `include/minesweeper.h` — 扫雷游戏
2. `src/ghost_shell.cpp` + `include/ghost_shell.h` — 隐藏调试终端
3. `src/safe_ram.cpp` + `include/safe_ram.h` — 64字节沙盒内存（扫雷参数 + Ghost Shell 读写）

## 需要修改的现有文件

### include/config.h
- 新增 `#define KEY_9 16`（ghost_shell 引用，当前版只到 KEY_8）。其余引脚/键值不动。

### include/game_common.h
- `GameMode` 枚举新增 `MODE_MINESWEEPER` 和 `MODE_SHELL`（放在 MODE_STUDY 之后，保持 MODE_PLANE 不删）。
  最终：`MODE_MENU, MODE_PIANO, MODE_PLANE, MODE_MINESWEEPER, MODE_STUDY, MODE_SHELL`

### include/keyboard.h + src/keyboard.cpp
- 加入按键事件环形队列 API：`keyQueuePush/Pop/Available/Flush` + `KEY_QUEUE_SIZE`。
- keyMap、扫描逻辑、防抖**不动**（与当前版一致）。

### src/main.cpp（改动最大，但小心保留现有修复）
保留：setup() 里的串口等待 + 分步日志、`audioUpdate()` 调用。
改动：
- 新增 include：minesweeper.h / ghost_shell.h / safe_ram.h
- setup() 加 `safeRAMInit();`（在 initAudio 前）。保留开机 `playTone(880,100)`。
- **菜单扩展为 4 项**：`items[]`/`descs[]` 各加扫雷一项；`menuSelection` 取模 `%3`→`%4`；drawMenu 的循环 `i<3`→`i<4`，卡片 Y 间距需重算（4 项要塞进 320 高，间距从 78 缩到约 58，标题栏后从 y=70 起）。
- handleMenu：加 Konami 码检测（↑↑↓↓←→←→ FIRE ENTER → MODE_SHELL）；ENTER 的 switch 加 case：0钢琴/1飞机/2扫雷/3学习。
- loop()：保留现有 `scanKeyboard()` 单次扫描 + `audioUpdate()` + `delay(16)` 结构**不变**；switch 仅新增 `MODE_MINESWEEPER`/`MODE_SHELL` 两个 case，保留 `MODE_PLANE`。
- **按键队列：用户决定先不接入 main**。队列 API 仍放进 keyboard.cpp/h 备用，但 loop 不调用。Konami 码里 plus 版用的 `keyQueueFlush()` 改为不调用（或调用也无害，但为最小化改动，进 Shell/游戏前不强制 flush）。

### platformio.ini
- 无需改依赖（Preferences 内置）。保留现有 monitor 配置和 disable_map。
- 当前有 `build_src_filter` 排除 keypad_test.cpp（之前键盘自检留的），保留该行；新增的 minesweeper/ghost_shell/safe_ram 会被 `+<*>` 自动包含。

## 用户已确认的决策
- **按键队列**：代码放进 keyboard.cpp/h 备用，但**不接入 main 主循环**（求稳）。
- **飞机大战**：完全不动 plane_game.cpp，保持现状（不合并 deltaTime 重构）。
- **Ghost Shell 入口**：保留 Konami 码隐藏入口（A A B B C D C D 9 0），不做成可见菜单项。

## 不做的事（明确排除）
- **不改钢琴判定**（当前版已修好，plus/(3) 版都是未修的旧逻辑，绝不能覆盖 piano_game.cpp）。
- **不引入 (3) 版的 deltaTime 物理重构**（飞机大战保持现状，确认不动）。
- **不替换 OLED 驱动为颜文字版**（位图版已满足新模块需求）。
- **不把按键队列接入 loop**（仅保留 API 备用）。

## 新配色（来自 (3) 版，低风险，最后做）
- 主菜单：标题栏 NAVY→YELLOW 底紫字、标题放大；选中卡片改绿色单边条。（会在 4 项新布局上重新套用）
- 学习助手 study_buddy.cpp：标题栏改浅薄荷绿、60min 红→橙、倒计时数字淡黄绿、完成横幅绿底放大。
- 颜色宏值不变，只改各处调用。注意统一 UTF-8 编码。

## 执行顺序
1. 复制 3 组新文件（minesweeper/ghost_shell/safe_ram）。
2. 改 config.h（KEY_9）、game_common.h（枚举）、keyboard.h+cpp（队列）。
3. 改 main.cpp（4项菜单 + 模式分发 + Konami + 队列 + safeRAMInit）。
4. 编译验证（pio run），修编译错。
5. 套用新配色（main 菜单 + study_buddy）。
6. 再次编译验证。
7. 询问用户是否烧录到 COM6 实测。

## 风险点
- 4 项菜单布局：320px 高要放标题栏+4卡片+底部提示，间距需调，可能需要目测微调坐标。
- Konami 码用 KEY_FIRE（物理9键）——确认当前 keyMap 物理9=KEY_FIRE，一致。
- 合并后 RAM/Flash 占用上升（扫雷 grid[256]+Preferences+ghost_shell 缓冲），当前 Flash 才 16%，余量充足。
