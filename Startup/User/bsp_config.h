/**
 ******************************************************************************
 * @file    bsp_config.h
 * @brief   【全部引脚和参数集中在本文件】要改接线，只改这一个文件
 *
 * @note    默认引脚方案（CH32V307VCT6）
 *          PWM 电机  : PA6  = TIM3_CH1（通用定时器，最省心，无需 MOE 使能）
 *          温度 ADC  : PA1  = ADC1_IN1（LM35 模拟输出）
 *          按键 ×4   : PB0 PB1 PB12 PB13（上拉输入，按下接 GND）
 *          LED  ×4   : PC0 PC1 PC2 PC3（档位1/2/3 + 自动模式指示）
 *          串口      : PA9(TX) PA10(RX) = USART1，115200，官方 debug.c 已配好
 *
 * @warning PA6 同时是 ADC1_IN6，PA1 同时是 TIM2_CH2 —— 本方案 PWM 用 TIM3、
 *          ADC 用 IN1，两者不冲突。★不要★把 PWM 改到 PA0/PA1 同时把 ADC 也放 PA 组低位，
 *          那样会撞车。改引脚前先看本文件末尾的"冲突自查表"。
 ******************************************************************************
 */
#ifndef __BSP_CONFIG_H
#define __BSP_CONFIG_H

#include "ch32v30x.h"

/* ==================== 电机 PWM ==================== */
#define MOTOR_PWM_GPIO_PORT      GPIOA
#define MOTOR_PWM_GPIO_RCC       RCC_APB2Periph_GPIOA
#define MOTOR_PWM_GPIO_PIN       GPIO_Pin_6      /* PA6 = TIM3_CH1 */
#define MOTOR_PWM_TIM            TIM3
#define MOTOR_PWM_TIM_RCC        RCC_APB1Periph_TIM3

/* ==================== 温度传感器（LM35 → ADC） ==================== */
#define TEMP_ADC_PORT            GPIOA
#define TEMP_ADC_RCC             RCC_APB2Periph_GPIOA
#define TEMP_ADC_PIN             GPIO_Pin_1      /* PA1 */
#define TEMP_ADC_CHANNEL         ADC_Channel_1   /* ★必须与上面引脚对应★ PA1=IN1, PA2=IN2 ... PA7=IN7 */
#define TEMP_ADC_UNIT            ADC1

/* ADC 时钟分频。官方例程用 Div8；若温度读数跳动大，改成 RCC_PCLK2_Div16 */
#define TEMP_ADC_CLK_DIV         RCC_PCLK2_Div8

/* LM35 灵敏度：10mV/℃。ADC 参考电压 3.3V、12 位
 * 温度(℃) = ADC值 × 3300 / 4096 / 10
 * ⚠️ LM35 最低工作电压是 4V，必须接开发板的 5V 引脚，不能接 3.3V */
#define TEMP_VREF_MV             3300.0f
#define TEMP_ADC_FULLSCALE       4096.0f
#define TEMP_MV_PER_DEGREE       10.0f

/* ==================== 按键（上拉输入，按下接 GND） ==================== */
#define KEY_PORT                 GPIOB
#define KEY_RCC                  RCC_APB2Periph_GPIOB
#define KEY_POWER_PIN            GPIO_Pin_0      /* PB0  开/关 */
#define KEY_UP_PIN               GPIO_Pin_1      /* PB1  档位 + */
#define KEY_DOWN_PIN             GPIO_Pin_12     /* PB12 档位 − */
#define KEY_MODE_PIN             GPIO_Pin_13     /* PB13 手动/自动切换 */

/* ==================== LED 指示（PC0~PC3） ==================== */
#define LED_PORT                 GPIOC
#define LED_RCC                  RCC_APB2Periph_GPIOC
#define LED_L1_PIN               GPIO_Pin_0      /* 档位1 */
#define LED_L2_PIN               GPIO_Pin_1      /* 档位2 */
#define LED_L3_PIN               GPIO_Pin_2      /* 档位3 */
#define LED_AUTO_PIN             GPIO_Pin_3      /* 自动模式 */

/* LED 点亮电平：0 = 低电平点亮（灌电流，板载 LED 常见接法）
 *            1 = 高电平点亮
 * ⚠️ 上电后先测一次：如果 LED 逻辑反了（该亮的不亮），把这个值改成另一个 */
#define LED_ACTIVE_LEVEL         1

/* ==================== 串口 ==================== */
#define DEBUG_BAUDRATE           115200

/* ==================== 主循环节拍 ==================== */
#define TICK_MS                  10u             /* 每 10ms 跑一次主循环 */
#define TEMP_SAMPLE_PERIOD_TICK  50u             /* 50 × 10ms = 500ms 采一次温度 */

/* =====================================================================
 * 【冲突自查表】改引脚前必看
 * ---------------------------------------------------------------------
 * PA0 = TIM2_CH1 / ADC_IN0      PA1 = TIM2_CH2 / ADC_IN1
 * PA2 = TIM2_CH3 / ADC_IN2      PA3 = TIM2_CH4 / ADC_IN3
 * PA6 = TIM3_CH1 / ADC_IN6      PA7 = TIM3_CH2 / ADC_IN7
 * PB0 = TIM3_CH3 / ADC_IN8      PB1 = TIM3_CH4 / ADC_IN9
 * PB6 = TIM4_CH1 / I2C1_SCL     PB7 = TIM4_CH2 / I2C1_SDA
 * PA9 = USART1_TX / TIM1_CH2    PA10 = USART1_RX / TIM1_CH3
 *
 * 规则：一个引脚只能干一件事。PWM 用了 PA6，就不能再拿 PA6 读 ADC。
 *
 * 【关于 PB0 / PB1 是不是冲突】——不冲突，可以放心用
 *   PB0/PB1 在复用表里确实标着 TIM3_CH3 / TIM3_CH4，
 *   但我们只用 TIM3 的 CH1（PA6）输出 PWM，CH3/CH4 完全没启用，
 *   所以 PB0/PB1 当普通按键输入用没有任何问题。
 *   ★除非★你以后改成用 TIM3_CH3 输出 PWM，那时 PB0 就不能再当按键了。
 *
 * 备用 PWM 方案：PA0 = TIM2_CH1
 *   若 PA6 在你们板子上被占用或排针没引出，改用 PA0：
 *   ① MOTOR_PWM_GPIO_PIN 改 GPIO_Pin_0
 *   ② MOTOR_PWM_TIM 改 TIM2、MOTOR_PWM_TIM_RCC 改 RCC_APB1Periph_TIM2
 *   ③ bsp_motor.c 里 TIM_OC1Init/TIM_SetCompare1 不变（TIM2_CH1 也是通道1）
 *   ④ 注意 PA0 = ADC_IN0，此时温度 ADC 仍用 PA1，不冲突
 * ===================================================================== */

#endif /* __BSP_CONFIG_H */
