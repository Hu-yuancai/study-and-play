/*
 * keyboard.cpp — 4x4 矩阵键盘驱动模块（实现文件）
 * ============================================================================
 * 【物理键盘布局与键值映射】
 *
 * 实际键盘的物理布局（大多数 4×4 薄膜键盘都是这样）：
 *   ┌────┬────┬────┬────┐
 *   │ 1  │ 2  │ 3  │ A  │  ← 第 1 行 (ROW1)
 *   ├────┼────┼────┼────┤
 *   │ 4  │ 5  │ 6  │ B  │  ← 第 2 行 (ROW2)
 *   ├────┼────┼────┼────┤
 *   │ 7  │ 8  │ 9  │ C  │  ← 第 3 行 (ROW3)
 *   ├────┼────┼────┼────┤
 *   │ *  │ 0  │ #  │ D  │  ← 第 4 行 (ROW4)
 *   └────┴────┴────┴────┘
 *     ↑     ↑     ↑     ↑
 *   第1列  第2列  第3列  第4列
 *  (COL1) (COL2)(COL3)(COL4)
 *
 * 映射到功能键值（定义在 config.h）：
 *   按键"A" → KEY_UP    (方向键上，钢琴轨道2)
 *   按键"B" → KEY_DOWN  (方向键下，钢琴轨道3)
 *   按键"C" → KEY_LEFT  (方向键左，钢琴轨道1) ← 注意：这里 KEY_FIRE 和 KEY_LEFT 互换
 *   按键"D" → KEY_RIGHT (方向键右，钢琴轨道4)
 *   按键"0" → KEY_ENTER (确认键)
 *   按键"*" → KEY_BACK  (返回键)
 *   按键"#" → KEY_ENTER (确认键，与 0 功能相同)
 *   按键"1" → KEY_1     (数字键)
 *   ... 等等
 *
 * 【如果你想自定义键盘布局】
 * 只需修改 keyMap[][] 数组即可。比如你想把第一行映射成不同的功能，
 * 改数组里对应的 KEY_XXX 值就行了。
 */

#include "keyboard.h"    // 自己的头文件

// ============================================================================
// 静态变量（只在 keyboard.cpp 内部可见，外部不能访问）
// ============================================================================

// rowPins[4] — 4 个行引脚对应的 GPIO 编号
// 从 config.h 读取，顺序：ROW1, ROW2, ROW3, ROW4
// 用 static 限定表示这个变量只在这里使用
static const uint8_t rowPins[4] = {KB_ROW1, KB_ROW2, KB_ROW3, KB_ROW4};

// colPins[4] — 4 个列引脚对应的 GPIO 编号
static const uint8_t colPins[4] = {KB_COL1, KB_COL2, KB_COL3, KB_COL4};

// lastKey — 记录上一次检测到的按键，用于实现"按住不重复触发"
// 如果当前按下的键与 lastKey 相同，scanKeyboard() 返回 KEY_NONE
static uint8_t lastKey = KEY_NONE;

// lastDebounce — 上次检测到按键变化的毫秒时间戳
// 配合 DEBOUNCE_MS 实现软件消抖
static unsigned long lastDebounce = 0;

// DEBOUNCE_MS — 消抖时间（毫秒）
// 两次有效按键之间至少间隔 50ms，小于此间隔的检测被忽略
// 你可以增大这个值让按键更"迟钝"，减小让它更"灵敏"
static const unsigned long DEBOUNCE_MS = 50;

// ============================================================================
// 键盘映射表
// ============================================================================
//
// keyMap[row][col] 定义了第 row 行、第 col 列交叉点的按键
// 按下时应该返回什么键值。
//
// 数组索引说明：
//   keyMap[0][0] = 左上角按键 (第1行第1列 = 物理按键"1") → 返回 KEY_1 (8)
//   keyMap[3][3] = 右下角按键 (第4行第4列 = 物理按键"D") → 返回 KEY_RIGHT (4)
//
// 【如何修改映射？】
// 比如你想让物理按键"A"变成"发射"键，只需把 keyMap[0][3] 从 KEY_UP 改成 KEY_FIRE
// 比如你想交换"上"和"下"，只需交换 keyMap[0][3] 和 keyMap[1][3] 的值
//
static const uint8_t keyMap[4][4] = {
  // 第0列(COL1)   第1列(COL2)   第2列(COL3)    第3列(COL4)
  { KEY_1,         KEY_2,        KEY_3,         KEY_UP    },  // 第0行(ROW1): 1 2 3 A
  { KEY_4,         KEY_5,        KEY_6,         KEY_DOWN  },  // 第1行(ROW2): 4 5 6 B
  { KEY_7,         KEY_8,        KEY_FIRE,      KEY_LEFT  },  // 第2行(ROW3): 7 8 9 C
  { KEY_BACK,      KEY_ENTER,    KEY_ENTER,     KEY_RIGHT }   // 第3行(ROW4): * 0 # D
};

// ============================================================================
// 核心函数实现
// ============================================================================

/*
 * initKeyboard() — 初始化键盘 GPIO
 *
 * 【引脚配置说明】
 * pinMode(pin, OUTPUT):      设为输出模式，程序可以控制这个引脚输出 HIGH(3.3V) 或 LOW(0V)
 * pinMode(pin, INPUT_PULLUP): 设为输入上拉模式，内部通过电阻连接到 3.3V
 *                             当没有按键按下时读到 HIGH，有按键按下并接地时读到 LOW
 *
 * 【初始状态】
 * 4 个行引脚 → OUTPUT + HIGH（不扫描时全为高电平）
 * 4 个列引脚 → INPUT_PULLUP（读列状态，上拉保证无按键时读到 HIGH）
 */
void initKeyboard() {
  // 初始化 4 个行引脚为输出模式，初始输出高电平
  for (int i = 0; i < 4; i++) {
    pinMode(rowPins[i], OUTPUT);         // 设为输出模式
    digitalWrite(rowPins[i], HIGH);      // 初始输出高电平 (3.3V)
  }

  // 初始化 4 个列引脚为输入上拉模式
  // 上拉意味着引脚内部通过一个电阻连到 3.3V
  // 所以没有按键按下时，digitalRead() 读到的是 HIGH
  for (int i = 0; i < 4; i++) {
    pinMode(colPins[i], INPUT_PULLUP);   // 设为输入上拉模式
  }
}

/*
 * scanKeyboard() — 逐行扫描，检测当前按下的按键
 *
 * 【算法详解（行列扫描法）】
 *
 * 伪代码：
 *   for 每一行(row):
 *     把当前行拉低(LOW)
 *     for 每一列(col):
 *       if 当前列读到 LOW:
 *         延时 10ms 再读一次（软件消抖）
 *         if 当前列还是 LOW:
 *           确定按键 = keyMap[row][col]
 *           if 这个键 != 上次按的键:
 *             记录为新按键，返回键值
 *           else:
 *             返回 KEY_NONE（按住不重复）
 *     把当前行恢复为高(HIGH)
 *   返回 KEY_NONE（没按键）
 *
 * 【为什么用两级消抖？】
 * 1. DEBOUNCE_MS (50ms) — 宏观消抖：两次采样间至少隔 50ms
 * 2. delay(10) 二次确认 — 微观消抖：在第一次读到 LOW 后等 10ms 再读
 * 这确保了只有真正的按键才会被检测到。
 *
 * @return 按下的键值，或 KEY_NONE (0) 表示没有新按键
 */
uint8_t scanKeyboard() {
  // 宏观消抖：距离上次有效按键不到 50ms，直接返回无按键
  // millis() 返回从开机到现在的毫秒数（unsigned long 类型）
  if (millis() - lastDebounce < DEBOUNCE_MS) {
    return KEY_NONE;
  }

  // 逐行扫描
  for (int row = 0; row < 4; row++) {
    // 第 1 步：把当前行拉低 (LOW)
    // 如果这一行有按键被按下，对应的列也会变成 LOW
    digitalWrite(rowPins[row], LOW);

    // 第 2 步：逐一检查 4 个列
    for (int col = 0; col < 4; col++) {
      // digitalRead() 读取引脚状态：HIGH(1) 或 LOW(0)
      if (digitalRead(colPins[col]) == LOW) {
        // 读到 LOW！可能有按键按下

        // 第 3 步：等待 10ms 后再次读取（微观消抖）
        delay(10);

        if (digitalRead(colPins[col]) == LOW) {
          // 第 4 步：确认按键有效

          // 先把行恢复为高电平（重要！否则影响后续扫描）
          digitalWrite(rowPins[row], HIGH);

          // 查表获取键值
          uint8_t key = keyMap[row][col];

          // 第 5 步：去重 — 如果和上次按键相同，忽略
          if (key != lastKey) {
            lastKey = key;               // 记录新按键
            lastDebounce = millis();     // 记录时间戳用于消抖
            return key;                  // 返回键值
          }
          // 相同按键持续按住，返回 KEY_NONE
          return KEY_NONE;
        }
      }
    }

    // 第 6 步：当前行扫描完毕，恢复为高电平
    digitalWrite(rowPins[row], HIGH);
  }

  // 第 7 步：所有行列都扫描完了，没有检测到按键
  // 清除 lastKey，这样下次按下同一个键时才能再次触发
  lastKey = KEY_NONE;
  return KEY_NONE;
}

/*
 * isKeyPressed(targetKey) — 检查某个特定键是否正在被按住
 *
 * 和 scanKeyboard() 的关键区别：
 * - scanKeyboard() 有去重逻辑，同一个键按住只触发一次
 * - isKeyPressed() 每次都重新扫描，按住时持续返回 true
 *
 * 适合场景：飞机游戏的持续方向移动（按住左键就持续左移）
 * 不适合场景：菜单选择（会一次跳过很多项）
 *
 * @param targetKey  要检查的键值，如 KEY_LEFT, KEY_FIRE 等
 * @return true 表示按键正在被按下，false 表示没有
 *
 * 【使用示例】
 *   // 持续移动：每次 loop 都检查
 *   if (isKeyPressed(KEY_LEFT)) {
 *     playerX -= PLAYER_SPEED;
 *   }
 */
bool isKeyPressed(uint8_t targetKey) {
  // 同样的行列扫描逻辑，但没有去重处理
  for (int row = 0; row < 4; row++) {
    digitalWrite(rowPins[row], LOW);     // 拉低当前行

    for (int col = 0; col < 4; col++) {
      // 如果列读到 LOW 且查表发现是目标按键
      if (digitalRead(colPins[col]) == LOW && keyMap[row][col] == targetKey) {
        digitalWrite(rowPins[row], HIGH);  // 恢复行状态
        return true;                       // 找到了，返回 true
      }
    }

    digitalWrite(rowPins[row], HIGH);     // 恢复行状态
  }
  return false;  // 没找到目标按键
}

// ============================================================================
// 按键事件队列 (环形缓冲区, 防漏键)
// ============================================================================
// 原理: 主循环每帧将 scanKeyboard() 结果入队, 各模块循环消费.
// 环形缓冲区 head=写入指针, tail=读取指针, 空=头尾相等, 满=头追尾

static uint8_t keyQueue[KEY_QUEUE_SIZE];
static volatile uint8_t keyQueueHead = 0;  // 写入位置
static volatile uint8_t keyQueueTail = 0;  // 读取位置

void keyQueuePush(uint8_t key) {
  if (key == KEY_NONE) return;
  uint8_t next = (keyQueueHead + 1) % KEY_QUEUE_SIZE;
  if (next == keyQueueTail) return;  // 队列满, 丢弃 (防止溢出)
  keyQueue[keyQueueHead] = key;
  keyQueueHead = next;
}

uint8_t keyQueuePop() {
  if (keyQueueHead == keyQueueTail) return KEY_NONE;  // 队列空
  uint8_t key = keyQueue[keyQueueTail];
  keyQueueTail = (keyQueueTail + 1) % KEY_QUEUE_SIZE;
  return key;
}

bool keyQueueAvailable() {
  return keyQueueHead != keyQueueTail;
}

void keyQueueFlush() {
  keyQueueHead = keyQueueTail = 0;  // 重置读写指针
}
