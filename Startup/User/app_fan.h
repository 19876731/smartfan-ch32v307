/**
 ******************************************************************************
 * @file    app_fan.h
 * @brief   风扇应用层：档位状态机 + 温度自动调速闭环
 * @note    这一层不直接碰寄存器，只调用 bsp_* 驱动，方便单独理解和调试。
 ******************************************************************************
 */
#ifndef __APP_FAN_H
#define __APP_FAN_H

#include "ch32v30x.h"

/* 工作模式 */
#define FAN_MODE_MANUAL   0u    /* 手动：档位由按键决定 */
#define FAN_MODE_AUTO     1u    /* 自动：档位由温度决定 */

void    Fan_Init(void);
/**
 * @brief  应用主处理，★每 10ms 调用一次★
 *         内部自己控制温度采样节奏（500ms 一次）
 */
void    Fan_Task(void);

uint8_t Fan_GetLevel(void);     /* 当前档位 0~3 */
uint8_t Fan_GetMode(void);      /* 当前模式 */
uint8_t Fan_GetPower(void);     /* 1=开机 0=关机 */
float   Fan_GetTemp(void);      /* 当前温度（℃） */

#endif /* __APP_FAN_H */
