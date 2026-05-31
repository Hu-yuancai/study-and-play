/*
 * plane_game.h — 飞机大战射击游戏模块（头文件）
 * ============================================================================
 * 【游戏简介】
 * 玩家控制屏幕底部的飞机，向从上方不断涌来的敌机射击。
 * 消灭敌机获得分数，被敌机碰到或让敌机逃出屏幕则失去生命。
 * 难度随时间和分数递增（敌机变快、变多、血量变高）。
 *
 * 【操作方式】
 * 方向键：移动飞机（支持按住连续移动）
 * 发射键 (C/ENTER)：发射子弹
 * 返回键 (BACK)：退出到主菜单
 */

#ifndef PLANE_GAME_H
#define PLANE_GAME_H

#include <Arduino.h>
#include "config.h"

/*
 * planeGameInit() — 初始化飞机大战
 * 重置分数、生命、关卡，创建玩家飞机
 * 在进入游戏时调用一次
 */
void planeGameInit();

/*
 * planeGameLoop(key) — 每帧调用，处理游戏逻辑
 * @param key  当前按下的键值（KEY_NONE 表示没有按键）
 */
void planeGameLoop(uint8_t key);

#endif  // PLANE_GAME_H 结束
