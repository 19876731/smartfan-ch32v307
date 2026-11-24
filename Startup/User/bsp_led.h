/**
 ******************************************************************************
 * @file    bsp_led.h
 * @brief   LED 档位指示驱动（4 个 LED：档位1 / 档位2 / 档位3 / 自动模式）
 * @note    不接 OLED 也能看出当前档位 —— 这是最稳的显示方案。
 *          LED 是加分项 OLED 的保底替代，务必先把它做通。
 ******************************************************************************
 */
#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "ch32v30x.h"

void Led_Init(void);

/**
 * @brief  按当前状态刷新 4 个 LED
 * @param  level: 0=关机, 1/2/3=手动档位
 * @param  auto_mode: 1=自动模式（点亮 AUTO 灯），0=手动模式
 * @note   自动模式下 level 仍会显示当前实际档位，两个信息同时可见
 */
void Led_Show(uint8_t level, uint8_t auto_mode);

void Led_AllOff(void);

#endif /* __BSP_LED_H */
