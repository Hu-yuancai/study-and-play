/*
 * study_buddy.h — 学习伴侣/番茄工作法计时器模块（头文件）
 * ============================================================================
 * 【功能简介】
 * 一个基于番茄工作法（Pomodoro Technique）的专注计时器。
 * 用户可以选择 25/30/45/60 分钟的专注时长。
 * 倒计时过程中，每 5 分钟会给出一次鼓励（OLED 表情变化）。
 * 计时结束后播放成功音效并显示完成界面。
 *
 * 【番茄工作法】
 * 弗朗西斯科·西里洛创立的时间管理方法：
 *   1. 选择一个任务
 *   2. 设定 25 分钟计时器
 *   3. 专注工作直到计时器响起
 *   4. 休息 5 分钟
 *   5. 每 4 个番茄钟后休息 15-30 分钟
 *
 * 【操作方式】
 * 上下键：选择时长
 * 确认键：开始计时 / 暂停 / 继续 / 确认完成
 * 返回键：退出到主菜单
 */

#ifndef STUDY_BUDDY_H
#define STUDY_BUDDY_H

#include <Arduino.h>
#include "config.h"

/*
 * studyBuddyInit() — 初始化学习伴侣
 * 进入时长选择界面
 */
void studyBuddyInit();

/*
 * studyBuddyLoop(key) — 每帧调用，处理计时器逻辑
 * @param key  当前按下的键值
 */
void studyBuddyLoop(uint8_t key);

#endif  // STUDY_BUDDY_H 结束
