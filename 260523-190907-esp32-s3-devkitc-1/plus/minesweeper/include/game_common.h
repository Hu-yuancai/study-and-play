/*
 * game_common.h — 游戏系统共享定义
 * ============================================================================
 * 【这个文件的作用】
 * 定义了所有游戏模块（主菜单、钢琴游戏、飞机大战、学习伴侣）
 * 之间共享的数据类型和全局变量。
 *
 * 【为什么要把共享定义放在一个单独的文件里？】
 * 假设没有这个文件，每个游戏模块都要自己定义 GameMode 枚举。
 * 如果将来要添加新游戏模式，你需要在 5 个文件里各改一次。
 * 集中定义 → 只改一个地方，所有模块自动同步。
 *
 * 【extern 关键字说明】
 * extern 告诉编译器："这个变量在别的文件里定义，这里只是声明引用它"。
 * 没有 extern 就是"在这里创建这个变量"。
 * 一个全局变量只能在一个 .cpp 文件中"创建"，但在多个文件中"引用"。
 *
 * 举个例子：
 *   game_common.h:  extern GameMode currentMode;  // 声明：存在这样一个全局变量
 *   main.cpp:       GameMode currentMode = MODE_MENU;  // 定义：真正创建并初始化
 *   piano_game.cpp: #include "game_common.h"  // 通过声明引用 main.cpp 中的变量
 */

#ifndef GAME_COMMON_H
#define GAME_COMMON_H

#include "config.h"   // 用到 FRAME_RATE 等配置

// ============================================================================
// 游戏模式枚举
// ============================================================================
//
// enum（枚举）是一种自定义数据类型，给一组整数起有意义的名字。
// 编译器会自动分配值：MODE_MENU=0, MODE_PIANO=1, MODE_PLANE=2, MODE_STUDY=3
//
// 使用枚举的好处：
//   if (currentMode == MODE_PIANO)  ← 一看就知道在检查"是否在钢琴模式"
//   if (currentMode == 1)           ← 1 是什么？还得去查文档
//
// 【如何添加新的游戏模式？】
// 1. 在下面加一个新的枚举值，如 MODE_COMPOSER
// 2. 在 main.cpp 的 handleMenu() 中加一个 case
// 3. 在 main.cpp 的 loop() 中加一个 case 调用新模式的 loop 函数
// 4. 在 drawMenu() 中加一个新的菜单项
//
enum GameMode {
  MODE_MENU,          // 0 — 主菜单
  MODE_PIANO,         // 1 — 钢琴块节奏游戏
  MODE_MINESWEEPER,   // 2 — 扫雷游戏
  MODE_STUDY,         // 3 — 学习伴侣
  MODE_SHELL          // 4 — Ghost Shell 调试终端
};

// ============================================================================
// 全局变量声明（具体定义在 main.cpp 中）
// ============================================================================

/*
 * currentMode — 当前所在的游戏模式
 * main.cpp 的 loop() 根据这个变量决定调用哪个模式的 update 函数
 * 任何模块都可以读写这个变量来切换模式
 * 比如：currentMode = MODE_MENU;  ← 回到主菜单
 */
extern GameMode currentMode;

/*
 * deltaTime — 当前帧耗时（秒）
 * 由 main.cpp 的 loop() 每帧计算, 所有游戏模块使用此值做速度控制。
 * 使用示例: playerX += speed * deltaTime;  // speed 单位 px/sec
 * 上限保护: 若帧耗时超过 0.1 秒则钳制为 0.016 (防卡顿后跳帧)
 */
extern float deltaTime;

// ============================================================================
// 全局函数声明（具体定义在 main.cpp 中）
// ============================================================================

/*
 * drawMenu() — 绘制主菜单
 * 被游戏模块在"返回主菜单"时调用，用来重绘 LCD 上的菜单界面
 */
extern void drawMenu();

#endif  // GAME_COMMON_H 结束
