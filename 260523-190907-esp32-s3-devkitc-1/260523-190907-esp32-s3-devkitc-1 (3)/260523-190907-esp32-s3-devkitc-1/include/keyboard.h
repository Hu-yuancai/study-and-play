/*
 * keyboard.h — 4x4 矩阵键盘驱动模块（头文件）
 * ============================================================================
 * 【这个模块的作用】
 * 驱动 4×4 矩阵键盘，扫描按键状态，返回按下了哪个键。
 *
 * 【什么是矩阵键盘？】
 * 普通的 16 个独立按键需要 16 个 GPIO 引脚。
 * 矩阵键盘通过 4 行 × 4 列 = 16 个交叉点的方式，
 * 只需要 4+4=8 个 GPIO 就能检测 16 个按键——
 * 节省了一半的引脚！
 *
 * 【电路原理】（重要！理解这个才能改代码）
 * 想象一个网格：
 *   - 4 条水平线（ROW1~ROW4，行）
 *   - 4 条垂直线（COL1~COL4，列）
 *   - 每个交叉点有一个按键
 *   - 按下按键 = 把对应行和列连接起来
 *
 * 扫描过程（行列扫描法）：
 *   1. 所有行设为高电平，所有列设为输入上拉（读出来是 HIGH）
 *   2. 依次把每一行拉低（LOW）
 *   3. 逐一读每一列
 *   4. 如果某列读到 LOW → 说明当前行+这一列的按键被按下了
 *      因为行 LOW 通过按键传导到了列
 *   5. 如果某列还是 HIGH → 这一行+这一列的按键没被按下
 *
 * 【为什么需要消抖？】
 * 机械按键在按下和释放的瞬间会产生"抖动"——
 * 触点在几毫秒内快速通断多次，导致程序检测到多次按键。
 * 消抖（Debounce）就是等信号稳定后再读取。
 * 常用的消抖方法：
 *   - 硬件消抖：并联电容（如 0.1μF）
 *   - 软件消抖：延迟一段时间再读（本项目使用的方法）
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Arduino.h>     // 提供 pinMode, digitalWrite, digitalRead 等基础函数
#include "config.h"      // 需要其中的引脚定义 (KB_ROW1-4, KB_COL1-4) 和键值 (KEY_UP等)

/*
 * initKeyboard() — 初始化键盘
 * 配置 4 个行引脚为输出（初始 HIGH），4 个列引脚为输入上拉。
 * 在 setup() 中调用一次即可。
 */
void initKeyboard();

/*
 * scanKeyboard() — 扫描键盘，返回当前按下的键值
 *
 * 【返回值】
 * 返回 KEY_NONE (0) 如果没有按键按下，
 * 返回 KEY_UP, KEY_DOWN, KEY_1... 等键值（定义在 config.h）如果有按键按下。
 *
 * 【重要特性——去重】
 * 同一个按键持续按住时，只会在第一次检测到时返回键值，
 * 之后返回 KEY_NONE，直到松开后再按。这避免了"按住一个键触发多次"。
 * （飞机游戏的持续移动用的是 isKeyPressed() 函数来额外检测）
 *
 * 【使用示例】
 *   uint8_t key = scanKeyboard();
 *   if (key == KEY_ENTER) {
 *     Serial.println("Enter 被按下!");
 *   }
 */
uint8_t scanKeyboard();

/*
 * isKeyPressed(targetKey) — 检查某个特定键是否正在被按住
 *
 * 与 scanKeyboard() 不同，这个函数每次都重新扫描，不做去重。
 * 适合需要持续检测按住的场景（如飞机游戏的方向键移动）。
 *
 * @param targetKey  要检查的键值（如 KEY_LEFT）
 * @return true 如果键被按下, false 如果没有
 *
 * 【使用示例】
 *   if (isKeyPressed(KEY_LEFT)) {
 *     playerX -= 1;  // 持续左移
 *   }
 */
bool isKeyPressed(uint8_t targetKey);

// ============================================================================
// 按键事件队列 (v6: 环形缓冲区, 防漏键)
// ============================================================================
// 主循环每帧将 scanKeyboard() 结果入队, 各模块从队列消费.
// 即使单帧耗时过长, 快速按键也不会丢失.

#define KEY_QUEUE_SIZE 8  // 环形缓冲区容量

void keyQueuePush(uint8_t key);       // 入队 (非 KEY_NONE 时调用)
uint8_t keyQueuePop();                // 出队, 返回 KEY_NONE 如果空
bool keyQueueAvailable();             // 队列是否有待处理事件
void keyQueueFlush();                 // 清空队列 (切换模式时使用)

#endif  // KEYBOARD_H 结束
