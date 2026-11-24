/**
 ******************************************************************************
 * @file    bsp_led.c
 * @brief   LED 指示驱动实现
 ******************************************************************************
 */
#include "bsp_led.h"
#include "bsp_config.h"

/* 写电平的小工具：按 LED_ACTIVE_LEVEL 自动决定是高电平点亮还是低电平点亮 */
static void Led_WritePin(uint16_t pin, uint8_t on)
{
    uint8_t level;

    if (on) {
        level = (LED_ACTIVE_LEVEL == 1u) ? 1u : 0u;
    } else {
        level = (LED_ACTIVE_LEVEL == 1u) ? 0u : 1u;
    }

    if (level) {
        GPIO_SetBits(LED_PORT, pin);
    } else {
        GPIO_ResetBits(LED_PORT, pin);
    }
}

void Led_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(LED_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = LED_L1_PIN | LED_L2_PIN | LED_L3_PIN | LED_AUTO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;   /* 推挽输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &GPIO_InitStructure);

    Led_AllOff();
}

void Led_AllOff(void)
{
    Led_WritePin(LED_L1_PIN,   0);
    Led_WritePin(LED_L2_PIN,   0);
    Led_WritePin(LED_L3_PIN,   0);
    Led_WritePin(LED_AUTO_PIN, 0);
}

void Led_Show(uint8_t level, uint8_t auto_mode)
{
    Led_WritePin(LED_L1_PIN,   (level == 1u) ? 1u : 0u);
    Led_WritePin(LED_L2_PIN,   (level == 2u) ? 1u : 0u);
    Led_WritePin(LED_L3_PIN,   (level == 3u) ? 1u : 0u);
    Led_WritePin(LED_AUTO_PIN, auto_mode ? 1u : 0u);
}
