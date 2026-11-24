/* oled.h  --  0.96寸 OLED (SSD1306, 128x64, I2C) 软件模拟 I2C 驱动
 *
 * 接线（默认）：
 *      OLED GND -> 开发板 GND
 *      OLED VCC -> 开发板 3.3V
 *      OLED SCL -> PB6
 *      OLED SDA -> PB7
 *
 * 想换引脚：只改下面 4 个宏即可。
 */

#ifndef __OLED_H
#define __OLED_H

#include "ch32v30x.h"
#include <stdint.h>

/* ---------- 引脚定义（改这里） ---------- */
#define OLED_SCL_PORT    GPIOB
#define OLED_SCL_PIN     GPIO_Pin_6
#define OLED_SDA_PORT    GPIOB
#define OLED_SDA_PIN     GPIO_Pin_7
#define OLED_GPIO_CLK    RCC_APB2Periph_GPIOB

/* ---------- I2C 地址 ---------- */
/* 屏不亮时先试 0x3C；若花屏/偏移再改 0x3D */
#define OLED_I2C_ADDR    0x3D

/* ---------- SH1106 兼容 ---------- */
/* 若画面整体左右偏移约 2 像素（左边缺一列/右边有竖条），
   说明底层其实是 SH1106，把下面的 0 改成 2 即可。 */
#define OLED_COL_OFFSET  0

/* ---------- 软件 I2C 速度 ---------- */
/* 数值越大越慢越稳。通信不稳定时把这个值调大（如 120）。 */
#define OLED_I2C_DELAY_N 40

/* ---------- API ---------- */
void OLED_Init(void);                                  /* 初始化，开机调一次 */
void OLED_Clear(void);                                 /* 全屏清黑 */
void OLED_SetPos(uint8_t x, uint8_t page);             /* x:列 0~127  page:页 0~7 */

void OLED_ShowChar(uint8_t x, uint8_t page, char ch);  /* 8x16 字符，占 2 页 */
void OLED_ShowString(uint8_t x, uint8_t page, const char *str);
void OLED_ShowInt(uint8_t x, uint8_t page, int32_t num); /* 显示整数（含负数） */

#endif /* __OLED_H */
