/**
 ******************************************************************************
 * @file    app_fan.c
 * @brief   风扇应用层实现：档位状态机 + 温度自动调速 + 滞回
 *
 * ==================== 状态机（答辩必讲） ====================
 *
 *   关机 ──POWER──→ 手动1档 ──UP──→ 手动2档 ──UP──→ 手动3档
 *    ↑                 │                                │
 *    │                 └──────DOWN───────←──────────────┘
 *    │
 *    └──POWER── 自动档 ←──MODE── 任意手动档
 *
 *   按键定义：
 *     POWER：关机 ↔ 开机（开机进 1 档）
 *     UP   ：档位 +1（3 档封顶）
 *     DOWN ：档位 −1（1 档封底，不会关机）
 *     MODE ：手动 ↔ 自动 切换（关机状态下按 MODE 无效）
 *
 * ==================== 温控闭环（本项目最大亮点） ====================
 *
 *   温度 → 目标档位 → 目标占空比 → PWM → 电机转速
 *
 *   阈值设计（可在下面 TEMP_TH_* 宏里调）：
 *     T < 24℃        → 0 档（停转）
 *     24℃ ≤ T < 28℃  → 1 档
 *     28℃ ≤ T < 32℃  → 2 档
 *     T ≥ 32℃        → 3 档
 *
 * ==================== 滞回 / 迟滞（Hysteresis）答辩加分点 ====================
 *
 *   问题：温度在阈值附近来回波动（比如 27.9 ↔ 28.1℃）时，
 *         档位会反复跳变，电机忽快忽慢，观感很差，也伤电机。
 *
 *   解决：升温阈值和降温阈值错开 1℃。
 *         升档要真的超过 28.0℃，降档要真的低于 27.0℃。
 *         中间这 1℃ 是"缓冲带"，进来容易出去难，档位就稳住了。
 *
 *   这是工业控制里的标准做法，评委很可能追问，务必讲清楚。
 ******************************************************************************
 */
#include "app_fan.h"
#include "bsp_config.h"
#include "bsp_motor.h"
#include "bsp_key.h"
#include "bsp_temp.h"
#include "bsp_led.h"
#include "debug.h"      /* printf 重定向 */
#include "app_print.h"  /* 浮点数拆分打印，见该文件说明 */

/* ==================== 温控阈值（★实测后按需微调★） ==================== */
#define TEMP_TH_L1_UP     24.0f   /* 升到 1 档 */
#define TEMP_TH_L1_DOWN   23.0f   /* 降到 0 档（比升档低 1℃ = 滞回） */
#define TEMP_TH_L2_UP     28.0f
#define TEMP_TH_L2_DOWN   27.0f
#define TEMP_TH_L3_UP     32.0f
#define TEMP_TH_L3_DOWN   31.0f

/* 档位 → 占空比 查表（下标就是档位号）
 * const 表示只读，放 Flash 不占 RAM */
static const uint16_t s_duty_table[4] = {
    0,                  /* 0 档：停转 */
    MOTOR_DUTY_LEVEL1,  /* 1 档 */
    MOTOR_DUTY_LEVEL2,  /* 2 档 */
    MOTOR_DUTY_LEVEL3   /* 3 档 */
};

/* ==================== 内部状态 ==================== */
static uint8_t s_power    = 0u;              /* 0=关机 1=开机 */
static uint8_t s_level    = 0u;              /* 当前档位 0~3 */
static uint8_t s_mode     = FAN_MODE_MANUAL; /* 手动/自动 */
static uint8_t s_manual_level = 1u;          /* 记住手动档的档位，切回来时用 */
static float   s_temp     = 25.0f;

/* 温度采样计时器（以 10ms tick 为单位） */
static uint8_t s_temp_tick = 0u;

/**
 * @brief  根据温度算出自动档应该处于第几档（含滞回）
 * @param  cur: 当前档位，用来决定"是在升还是在降"，这是滞回能生效的关键
 */
static uint8_t TempToLevel(float t, uint8_t cur)
{
    /* 升档：越过"升档阈值"，一次只升一档 */
    if (cur < 3u) {
        float up_th;
        switch (cur) {
            case 0u:  up_th = TEMP_TH_L1_UP; break;
            case 1u:  up_th = TEMP_TH_L2_UP; break;
            default:  up_th = TEMP_TH_L3_UP; break;   /* cur == 2 */
        }
        if (t >= up_th) { return cur + 1u; }
    }

    /* 降档：低于"降档阈值"，一次只降一档
     * 降档阈值比升档阈值低 1℃，中间这 1℃ 就是滞回缓冲带 */
    if (cur > 0u) {
        float down_th;
        switch (cur) {
            case 3u:  down_th = TEMP_TH_L3_DOWN; break;
            case 2u:  down_th = TEMP_TH_L2_DOWN; break;
            default:  down_th = TEMP_TH_L1_DOWN; break;   /* cur == 1 */
        }
        if (t < down_th) { return cur - 1u; }
    }

    return cur;   /* 温度在缓冲带内，档位保持不变 —— 这就是滞回生效的地方 */
}

/**
 * @brief  把档位变化落实到电机和 LED
 */
static void ApplyState(void)
{
    uint8_t eff_level;

    /* 关机时强制 0 档 */
    eff_level = s_power ? s_level : 0u;

    Motor_RampTo(s_duty_table[eff_level]);     /* 渐变过渡，不是跳变 */
    Led_Show(eff_level, (s_power && s_mode == FAN_MODE_AUTO) ? 1u : 0u);
}

void Fan_Init(void)
{
    s_power        = 0u;
    s_level        = 0u;
    s_mode         = FAN_MODE_MANUAL;
    s_manual_level = 1u;
    s_temp_tick    = 0u;
    s_temp         = 25.0f;

    Motor_SetDuty(0);
    Led_AllOff();
}

/**
 * @brief  处理按键事件
 */
static void HandleKey(uint8_t evt)
{
    if (evt == 0u) { return; }

    /* ---- 电源键 ---- */
    if (evt & KEY_ID_POWER) {
        if (s_power) {
            s_power = 0u;
            s_level = 0u;
            printf("[KEY] POWER -> OFF\r\n");
        } else {
            s_power = 1u;
            /* 开机：手动模式进上次记住的档位；自动模式先给 1 档，等温度采样修正 */
            s_level = (s_mode == FAN_MODE_AUTO) ? 1u : s_manual_level;
            printf("[KEY] POWER -> ON, level=%d mode=%s\r\n",
                   s_level, (s_mode == FAN_MODE_AUTO) ? "AUTO" : "MANUAL");
        }
        ApplyState();
    }

    /* 关机状态下，其余按键不响应（避免关机了还在改档位） */
    if (!s_power) { return; }

    /* ---- 模式切换键 ---- */
    if (evt & KEY_ID_MODE) {
        if (s_mode == FAN_MODE_MANUAL) {
            s_manual_level = s_level;       /* 记住手动档位 */
            s_mode = FAN_MODE_AUTO;
            printf("[KEY] MODE -> AUTO (temp=%d.%dC)\r\n",
                   PRINT_INT(s_temp), PRINT_FRAC(s_temp));
        } else {
            s_mode  = FAN_MODE_MANUAL;
            s_level = s_manual_level;       /* 切回记住的手动档位 */
            printf("[KEY] MODE -> MANUAL, level=%d\r\n", s_level);
        }
        ApplyState();
    }

    /* ---- 档位 + ----
     * 只在手动模式下有效；自动模式下档位由温度决定，按键改不了 */
    if (evt & KEY_ID_UP) {
        if (s_mode == FAN_MODE_MANUAL) {
            if (s_level < 3u) {
                s_level++;
                s_manual_level = s_level;
                printf("[KEY] UP -> level=%d duty=%d\r\n", s_level, s_duty_table[s_level]);
                ApplyState();
            }
        } else {
            printf("[KEY] UP ignored: AUTO mode\r\n");
        }
    }

    /* ---- 档位 − ---- */
    if (evt & KEY_ID_DOWN) {
        if (s_mode == FAN_MODE_MANUAL) {
            if (s_level > 1u) {             /* 1 档封底，不按到关机（关机用 POWER） */
                s_level--;
                s_manual_level = s_level;
                printf("[KEY] DOWN -> level=%d duty=%d\r\n", s_level, s_duty_table[s_level]);
                ApplyState();
            }
        } else {
            printf("[KEY] DOWN ignored: AUTO mode\r\n");
        }
    }
}

void Fan_Task(void)
{
    /* 1. 扫按键（每个 tick 都扫，10ms 一次，响应够快） */
    HandleKey(Key_Scan());

    /* 2. 电机渐变（每个 tick 都走一步） */
    Motor_RampUpdate();

    /* 3. 温度采样：每 50 个 tick = 500ms 一次
     *    为什么不每个 tick 都采？
     *    ① ADC 一次要采 5 个数做滤波，耗时几毫秒，太频繁会拖慢主循环
     *    ② 温度本身变化很慢，500ms 足够
     *    ③ 少采几次 = 少受电机干扰 */
    if (s_power) {
        s_temp_tick++;
        if (s_temp_tick >= TEMP_SAMPLE_PERIOD_TICK) {
            s_temp_tick = 0u;
            s_temp = Temp_Update();

            /* 4. 自动模式：用新温度重算档位 */
            if (s_mode == FAN_MODE_AUTO) {
                uint8_t new_level = TempToLevel(s_temp, s_level);
                if (new_level != s_level) {
                    printf("[AUTO] temp=%d.%dC level %d -> %d\r\n",
                           PRINT_INT(s_temp), PRINT_FRAC(s_temp), s_level, new_level);
                    s_level = new_level;
                    ApplyState();
                }
            }
        }
    } else {
        /* 关机时清零计时器，保证开机后第一个采样周期是完整的 */
        s_temp_tick = 0u;
    }
}

uint8_t Fan_GetLevel(void)
{
    return s_power ? s_level : 0u;
}

uint8_t Fan_GetMode(void)
{
    return s_mode;
}

uint8_t Fan_GetPower(void)
{
    return s_power;
}

float Fan_GetTemp(void)
{
    return s_temp;
}
