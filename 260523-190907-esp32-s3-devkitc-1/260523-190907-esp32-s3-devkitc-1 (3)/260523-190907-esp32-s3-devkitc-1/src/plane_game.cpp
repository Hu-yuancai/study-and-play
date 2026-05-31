/*
 * plane_game.cpp — 飞机大战射击游戏 (帧缓冲渲染 + OLED 表情 + 音效)
 * ============================================================================
 * 【游戏玩法】
 * - 你控制屏幕底部的飞机，向上方射击来袭的敌机
 * - 方向键移动飞机，C/ENTER 键发射子弹
 * - 消灭敌机获得分数，被敌机碰到或让敌机逃出屏幕则扣命
 * - 每 10 秒(600帧)关卡升级，敌机生成加速
 * - 3 条命用完游戏结束，按 ENTER 重新开始
 *
 * 【游戏架构】
 * 所有游戏对象用结构体数组管理（子弹池 + 敌机池），
 * 避免频繁 new/delete（嵌入式系统不推荐动态内存）。
 *
 * 【碰撞检测算法】
 * 使用 AABB (Axis-Aligned Bounding Box) 矩形重叠检测：
 *   两个矩形有交集 ⟺ 它们在 X 轴和 Y 轴上都有重叠
 *   !(A.right < B.left || A.left > B.right || A.top > B.bottom || A.bottom < B.top)
 *
 * 【v4 优化】DeltaTime 速度控制:
 *   所有运动速度改用 px/sec (每秒像素), 乘以全局 deltaTime 实现帧率无关物理。
 *   关卡升级从帧计数改为实际时间累积。
 */

#include "plane_game.h"
#include "game_common.h"   // currentMode, drawMenu, deltaTime
#include "lcd_driver.h"   // LCD 绘图函数
#include "oled_driver.h"  // OLED 表情和文字
#include "audio.h"        // SFX_fire, SFX_explode
#include "keyboard.h"     // isKeyPressed (持续按键检测)

// ============================================================================
// 游戏参数（速度单位: px/sec, 原 px/frame × 60fps）
// ============================================================================

#define MAX_BULLETS   8
#define MAX_ENEMIES   6
#define PLAYER_W      16
#define PLAYER_H      16
#define BULLET_SPEED  720    // px/sec (原 12px/frame × 60fps)
#define ENEMY_SPEED   180    // px/sec (原 3px/frame × 60fps)
#define PLAYER_SPEED  360    // px/sec (原 6px/frame × 60fps)
#define CONTINUOUS_SPEED 60  // px/sec 连续按住移动 (原 1px/frame × 60fps)

// ============================================================================
// 数据结构
// ============================================================================

/*
 * Bullet — 子弹结构体
 * 用"对象池"管理：active=true 表示子弹存在，false 表示空闲可复用
 * 发射子弹时找一个空闲槽位填充，飞出屏幕后标记为 inactive
 */
struct Bullet {
  int x, y;         // 子弹坐标（x=水平位置, y=垂直位置）
  bool active;      // 是否激活（true=在空中飞行, false=空闲）
};

/*
 * Enemy — 敌机结构体
 * 同样用对象池管理。hp 大于 1 的敌机需要多次命中才能消灭。
 */
struct Enemy {
  int x, y;         // 左上角坐标
  int w, h;         // 宽高（随机生成，大小不一）
  int hp;           // 生命值（>1 的敌机需要多枪）
  bool active;      // 是否激活
};

// ============================================================================
// 全局游戏状态
// ============================================================================

static int playerX, playerY;
static int lives;
static int score;
static bool gameOver;
static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static unsigned long lastEnemySpawn;
static unsigned long enemySpawnInterval;
static int level;
static float levelUpTimer;            // 关卡升级时间累积 (秒)
static unsigned long lastOledUpdate;

// ============================================================================
// 玩家飞机位图（16×16 像素，PROGMEM 存储）
// ============================================================================
//
// 下面是一个简单的三角箭头形状飞机：
//         ██
//        ████
//       ██████
//      ████████
//     ██████████
//    ████████████
//   ██████████████
//  ████████████████
//  ████████████████
//   ███ ██████ ███     ← 这里凹进去一块，形成机翼效果
//    ████████████
//
// 如果你想改飞机外观，可以用 16×16 像素编辑器重新设计，
// 然后用在线工具导出为字节数组替换下面的数据。

static const uint8_t PROGMEM playerBitmap[] = {
  0x01,0x80, 0x01,0x80, 0x03,0xC0, 0x03,0xC0,
  0x07,0xE0, 0x07,0xE0, 0x0F,0xF0, 0x0F,0xF0,
  0x1F,0xF8, 0x1F,0xF8, 0x3F,0xFC, 0x3F,0xFC,
  0x7F,0xFE, 0x7F,0xFE, 0x3B,0xDC, 0x1F,0xF8
};

// ============================================================================
// 函数实现
// ============================================================================

/*
 * planeGameInit() — 初始化/重置游戏
 *
 * 设置玩家的初始位置（屏幕底部中央），3 条命，0 分，第 1 关。
 * 清空子弹池和敌机池。
 * OLED 显示游戏说明。
 */
void planeGameInit() {
  // 玩家起始位置：水平居中，垂直在底部留 2 像素边距
  playerX = LCD_WIDTH / 2 - PLAYER_W / 2;
  playerY = LCD_HEIGHT - PLAYER_H - 2;

  lives = 3;                   // 3 条命
  score = 0;                   // 从 0 分开始
  gameOver = false;
  level = 1;
  levelUpTimer = 0;            // 关卡升级计时归零
  enemySpawnInterval = 1500;

  // 记录当前时间戳
  lastEnemySpawn = millis();
  lastOledUpdate = 0;

  // 清空子弹池：把所有子弹标记为 inactive
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  // 清空敌机池
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;

  // OLED 显示操作说明
  oledClear();
  oledDrawString(10, 0,  "Plane Battle");
  oledDrawString(10, 18, "Arrows:Move");
  oledDrawString(10, 36, "C:Fire");
  oledRefresh();
}

/*
 * fireBullet() — 发射一颗子弹
 *
 * 在子弹池中找一个空闲槽位，
 * 把子弹放在玩家飞机顶部中央位置，
 * 激活它并播放发射音效。
 *
 * 如果子弹池满了（MAX_BULLETS 颗都在飞行），
 * 就不能再发射——这是有意设计的限制。
 */
static void fireBullet() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].active = true;
      bullets[i].x = playerX + PLAYER_W / 2;  // 飞机水平中心
      bullets[i].y = playerY - 4;              // 飞机上方 4 像素
      playSFX_fire();  // 播放"piu~" 发射音效
      return;          // 只发射一颗
    }
  }
  // 如果走到这里说明子弹池满了，静默忽略
}

/*
 * spawnEnemy() — 随机生成一架敌机
 *
 * 敌机属性：
 *   - 宽度：随机 15~30 像素
 *   - 高度：固定 20 像素
 *   - 水平位置：随机（确保不超出屏幕）
 *   - 生成位置：屏幕顶部边缘外（y = -高度）
 *   - 血量：1 + 关卡/3（第 1-2 关 = 1HP, 3-5 关 = 2HP...）
 */
static void spawnEnemy() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].active = true;
      enemies[i].w = 15 + random(15);      // 随机宽度 15~29
      enemies[i].h = 20;                    // 固定高度
      enemies[i].x = random(LCD_WIDTH - enemies[i].w); // 随机X位置
      enemies[i].y = -enemies[i].h;         // 从屏幕上方外进入
      enemies[i].hp = 1 + level / 3;        // 随关卡增加血量
      return;
    }
  }
}

/*
 * updateBullets() — 更新所有子弹位置
 *
 * 每帧让所有活跃子弹向上移动 BULLET_SPEED 像素。
 * 飞出屏幕顶部(y < 0)的子弹标记为 inactive。
 */
static void updateBullets() {
  // 使用 deltaTime: y -= speed(px/sec) * dt(秒)
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      bullets[i].y -= BULLET_SPEED * deltaTime;
      if (bullets[i].y < 0) bullets[i].active = false;
    }
  }
}

/*
 * updateEnemies() — 更新所有敌机位置
 *
 * 每帧让所有活跃敌机向下移动。速度随关卡增加。
 * 逃出屏幕底部(y > LCD_HEIGHT)的敌机：扣一条命，标记 inactive。
 * 生命 ≤0 时游戏结束。
 */
static void updateEnemies() {
  float spd = ENEMY_SPEED + level * 30;  // px/sec, 随关卡增加
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      enemies[i].y += spd * deltaTime;
      if (enemies[i].y > LCD_HEIGHT) {
        enemies[i].active = false;
        lives--;
        if (lives <= 0) gameOver = true;
      }
    }
  }
}

/*
 * checkCollisions() — 碰撞检测
 *
 * 检测两大类碰撞：
 *   1. 子弹 vs 敌机：子弹命中敌机，扣敌机 HP，HP≤0 则消灭
 *   2. 玩家 vs 敌机：玩家飞机碰到敌机，敌机消失 + 扣命
 *
 * 使用 AABB 矩形重叠检测。
 */
static void checkCollisions() {
  // --- 子弹 vs 敌机 ---
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;     // 跳过空闲子弹

    for (int j = 0; j < MAX_ENEMIES; j++) {
      if (!enemies[j].active) continue;   // 跳过空闲敌机

      // AABB 碰撞检测：子弹点是否在敌机矩形内
      if (bullets[i].x >= enemies[j].x &&
          bullets[i].x <= enemies[j].x + enemies[j].w &&
          bullets[i].y >= enemies[j].y &&
          bullets[i].y <= enemies[j].y + enemies[j].h) {

        bullets[i].active = false;        // 子弹消失
        enemies[j].hp--;                   // 敌机扣血

        if (enemies[j].hp <= 0) {
          enemies[j].active = false;       // 敌机被消灭
          score += 10 * level;             // 加分（关卡越高加分越多）
          playSFX_explode();               // 播放爆炸音效
        }
        break;  // 一颗子弹只能命中一架敌机
      }
    }
  }

  // --- 玩家 vs 敌机 ---
  for (int j = 0; j < MAX_ENEMIES; j++) {
    if (!enemies[j].active) continue;

    // AABB 碰撞检测：两个矩形的交集
    if (enemies[j].x < playerX + PLAYER_W &&
        enemies[j].x + enemies[j].w > playerX &&
        enemies[j].y < playerY + PLAYER_H &&
        enemies[j].y + enemies[j].h > playerY) {

      enemies[j].active = false;   // 敌机消失
      lives--;                      // 扣命
      if (lives <= 0) gameOver = true;
    }
  }
}

/*
 * handleInput(key) — 处理玩家输入
 *
 * 方向键移动有两种模式：
 *   1. 离散模式 (KEY_UP/DOWN/LEFT/RIGHT)：每次按键移动 PLAYER_SPEED 像素
 *   2. 连续模式 (isKeyPressed)：适合按住方向键持续移动，每帧移 1 像素
 *
 * 发射：KEY_FIRE 或 KEY_ENTER
 * 边界限制：玩家不能移出屏幕
 */
static void handleInput(uint8_t key) {
  // --- 离散方向移动 (px/sec × dt) ---
  float move = PLAYER_SPEED * deltaTime;
  if (key == KEY_LEFT  && playerX > 0)                    playerX -= move;
  if (key == KEY_RIGHT && playerX < LCD_WIDTH - PLAYER_W) playerX += move;
  if (key == KEY_UP    && playerY > 0)                    playerY -= move;
  if (key == KEY_DOWN  && playerY < LCD_HEIGHT - PLAYER_H) playerY += move;

  // --- 发射 ---
  if (key == KEY_FIRE || key == KEY_ENTER) fireBullet();

  // --- 连续方向移动 (isKeyPressed 检测按住, px/sec × dt) ---
  float cMove = CONTINUOUS_SPEED * deltaTime;
  if (isKeyPressed(KEY_LEFT)  && playerX > 0)                    playerX -= cMove;
  if (isKeyPressed(KEY_RIGHT) && playerX < LCD_WIDTH - PLAYER_W) playerX += cMove;
}

/*
 * render() — 渲染游戏画面
 *
 * 绘制顺序（从背景到前景）：
 *   1. 清屏
 *   2. 画玩家飞机（位图）
 *   3. 画子弹（垂直线段）
 *   4. 画敌机（实心矩形）
 *   5. 画 HUD（生命/分数/关卡）
 *   6. lcdRefresh() 推到屏幕
 */
static void render() {
  lcdClear();  // 清空画布（默认黑色背景）

  // --- 玩家飞机 ---
  lcdDrawBitmap(playerX, playerY, PLAYER_W, PLAYER_H, playerBitmap);

  // --- 子弹（画成短垂直线，像激光束）---
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      // 从子弹位置向下画一条 12 像素长的线
      lcdDrawLine(bullets[i].x, bullets[i].y,
                  bullets[i].x, bullets[i].y + 12);
    }
  }

  // --- 敌机（实心矩形，默认白色）---
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      lcdFillRect(enemies[i].x, enemies[i].y,
                  enemies[i].w, enemies[i].h);
    }
  }

  // --- HUD (Head-Up Display) ---
  char buf[20];
  snprintf(buf, sizeof(buf), "L:%d S:%d Lv:%d", lives, score, level);
  lcdDrawString(0, 0, buf);  // 左上角显示

  lcdRefresh();  // 一次性推到屏幕（避免闪烁）
}

/*
 * updateOLED() — 更新 OLED 副屏
 *
 * 显示当前分数和生命数在顶部/底部边框，
 * 并让待机表情眨眼。
 * 每 200ms 更新一次（避免过于频繁的 I2C 通信）。
 */
static void updateOLED() {
  char top[16], bottom[16];
  snprintf(top, sizeof(top), "Score:%d", score);
  snprintf(bottom, sizeof(bottom), "Lives:%d", lives);
  oledDrawBorderText(top, bottom);

  // 1 秒眨眼周期：睁眼 500ms → 闭眼 500ms → 睁眼 500ms...
  oledShowBlinkingFace(FACE_IDLE_A, FACE_IDLE_B, 1000);
}

// ============================================================================
// 公共接口
// ============================================================================

/*
 * planeGameLoop(key) — 每帧调用的主函数
 *
 * 【执行流程】
 * 1. 检查返回键 → 回到主菜单
 * 2. 如果游戏结束 → 显示结算画面 + 等待 ENTER 重来
 * 3. 帧率控制 → 确保稳定的帧间隔
 * 4. 处理输入 → 移动/发射
 * 5. 更新子弹 → 向上移动
 * 6. 更新敌机 → 向下移动
 * 7. 碰撞检测 → 子弹命中/玩家被撞
 * 8. 生成新敌机 → 按间隔定时生成
 * 9. 关卡升级 → 每 600 帧(约10秒)升一级
 * 10. 渲染画面 → LCD + OLED
 *
 * @param key  当前按键值
 */
void planeGameLoop(uint8_t key) {
  // --- 返回主菜单 ---
  if (key == KEY_BACK) {
    currentMode = MODE_MENU;
    drawMenu();
    oledShowWelcome();
    return;
  }

  // --- 游戏结束处理 ---
  if (gameOver) {
    // 渲染结算画面
    lcdClear();
    lcdDrawString(80, 30, "GAME OVER");

    char buf[32];
    snprintf(buf, sizeof(buf), "Final Score: %d", score);
    lcdDrawString(60, 80, buf);

    snprintf(buf, sizeof(buf), "Level: %d", level);
    lcdDrawString(80, 120, buf);

    lcdDrawString(60, 180, "ENTER to restart");
    lcdRefresh();

    // OLED 显示难过表情
    oledDrawBorderText("Game Over", "ENTER=restart");
    oledShowFace(FACE_PIANO_MISS);

    // 按 ENTER 重新开始
    if (key == KEY_ENTER) planeGameInit();
    return;
  }

  // --- DeltaTime 速度控制: 不再使用固定帧率限制 ---
  // 所有运动速度已改为 px/sec × deltaTime, 帧率变化不影响游戏速度

  // --- 游戏逻辑更新 ---
  handleInput(key);       // 1. 处理玩家输入
  updateBullets();        // 2. 子弹向上移动
  updateEnemies();        // 3. 敌机向下移动
  checkCollisions();      // 4. 碰撞检测

  // --- 敌机生成 ---
  if (millis() - lastEnemySpawn > enemySpawnInterval) {
    spawnEnemy();                      // 生成新敌机
    lastEnemySpawn = millis();         // 重置计时器
  }

  // --- 关卡升级 (每 10 秒, 时间累积) ---
  levelUpTimer += deltaTime;
  if (levelUpTimer >= 10.0f) {
    levelUpTimer -= 10.0f;
    level++;
    enemySpawnInterval = max(400UL, enemySpawnInterval - 200);
  }

  // --- 渲染 ---
  render();  // LCD 游戏画面

  // OLED 每 200ms 刷新一次（避免频繁 I2C 通信）
  if (millis() - lastOledUpdate > 200) {
    updateOLED();
    lastOledUpdate = millis();
  }
}
