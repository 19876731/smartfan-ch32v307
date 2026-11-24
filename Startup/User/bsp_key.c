/**
 ******************************************************************************
 * @file    bsp_key.c
 * @brief   按键驱动实现（软件消抖 + 边沿检测）
 *
 * 【消抖原理，答辩必问】
 *   机械按键的触点在按下瞬间会弹跳几毫秒，电平在 0/1 之间快速抖动。
 *   如果直接读电平，一次按键可能被识别成好几次。
 *   解决办法：检测到"按下"后不立刻相信，连续 N 次采样都是按下才算数。
 *   本项目 N=2，每 10ms 采一次，即稳定 20ms 才确认 —— 抖动一般只有 5~10ms。
 *
 * 【为什么用"计数"而不是"delay 消抖"】
 *   常见教程写 if(按下){ delay_ms(20); if(还按着){...} }
 *   这样 delay 期间整个程序卡死，电机渐变、温度采样、显示全部停摆。
 *   本项目用状态计数，不占用时间，主循环照样跑 —— 这是"非阻塞"设计。
 ******************************************************************************
 */
#include "bsp_key.h"
#include "bsp_config.h"

/* 每个按键的消抖计数器 */
static uint8_t s_cnt[4]   = {0, 0, 0, 0};
/* 每个按键的稳定状态：0=松开，1=按下 */
static uint8_t s_state[4] = {0, 0, 0, 0};

/**
 * @brief  初始化按键 GPIO 为上拉输入
 */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(KEY_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = KEY_POWER_PIN | KEY_UP_PIN |
                                  KEY_DOWN_PIN  | KEY_MODE_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   /* IPU = Input Pull-Up 上拉输入 */
    GPIO_Init(KEY_PORT, &GPIO_InitStructure);
}

/**
 * @brief  读某个引脚的原始状态（已按"按下=0"取反，返回 1 表示按下）
 */
static uint8_t Key_ReadRaw(uint16_t pin)
{
    return (GPIO_ReadInputDataBit(KEY_PORT, pin) == Bit_RESET) ? 1u : 0u;
}

/**
 * @brief  扫描单个按键，返回是否产生"刚按下"事件
 */
static uint8_t Key_ProcessOne(uint8_t idx, uint16_t pin)
{
    uint8_t raw = Key_ReadRaw(pin);

    if (raw == 1u) {
        /* 读到低电平，可能是真按下，也可能是抖动 */
        if (s_cnt[idx] < KEY_DEBOUNCE_CNT) {
            s_cnt[idx]++;
        } else if (s_state[idx] == 0u) {
            /* 连续 N 次确认为按下，且之前是松开状态 → 产生一次按下事件 */
            s_state[idx] = 1u;
            return 1u;
        }
    } else {
        /* 读到高电平：清零计数，回到松开状态 */
        s_cnt[idx]   = 0u;
        s_state[idx] = 0u;
    }
    return 0u;
}

uint8_t Key_Scan(void)
{
    uint8_t evt = 0;

    if (Key_ProcessOne(0, KEY_POWER_PIN)) { evt |= KEY_ID_POWER; }
    if (Key_ProcessOne(1, KEY_UP_PIN))    { evt |= KEY_ID_UP;    }
    if (Key_ProcessOne(2, KEY_DOWN_PIN))  { evt |= KEY_ID_DOWN;  }
    if (Key_ProcessOne(3, KEY_MODE_PIN))  { evt |= KEY_ID_MODE;  }

    return evt;
}
