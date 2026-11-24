/**
 ******************************************************************************
 * @file    bsp_motor.h
 * @brief   直流电机 PWM 调速驱动（CH32V307 + L298N 驱动模块）
 * @note    引脚定义在 bsp_config.h，改接线只改那一个文件。
 *          本方案使用 PA6 = TIM3_CH1。
 *
 *          【为什么选 TIM3 而不是 TIM1】
 *          TIM1 是"高级定时器"，输出 PWM 前必须额外调用
 *          TIM_CtrlPWMOutputs(TIM1, ENABLE) 打开 MOE 主输出使能，
 *          新手最容易漏掉这一步，结果是"代码没报错、灯也亮、电机就是不转"。
 *          TIM3 是通用定时器，没有这个坑 —— 这是为稳定性做的选择。
 ******************************************************************************
 */
#ifndef __BSP_MOTOR_H
#define __BSP_MOTOR_H

#include "ch32v30x.h"

/* ================= PWM 频率参数 ================= */

/* 定时器时钟。CH32V307 系统时钟 144MHz，APB1 二分频后定时器时钟 ×2 = 144MHz
 * ⚠️ 如果你们板子的 system_ch32v30x.c 里主频不是 144MHz，把这里改成对应值
 *    （比如主频 72MHz 就写 72000000u） */
#define MOTOR_TIM_CLK_HZ     96000000u

/* PWM 周期计数值。ARR=999 → 占空比 0~999 对应 0%~99.9%，精度足够 */
#define MOTOR_PWM_ARR        999u

/* 目标 PWM 频率。L298N 推荐 1kHz~20kHz。
 * 1kHz：电机安静、损耗小，新手首选
 * 若电机啸叫明显，改成 2000u（2kHz）再试 */
#define MOTOR_PWM_FREQ_HZ    1000u

/* 预分频值由上面三个参数自动算出，不用手改 */
#define MOTOR_TIM_PSC        (MOTOR_TIM_CLK_HZ / MOTOR_PWM_FREQ_HZ / (MOTOR_PWM_ARR + 1u) - 1u)

/* ================= 档位占空比（★实测时必须微调★） =================
 * L298N 是 BJT 工艺，导通压降随电流变化（风扇电机 0.2~0.5A 时约 1.4~2V），
 * 12V 电机实际只拿到约 10V，所以 1 档不能设太低，
 * 否则电机启动不了（只会嗡嗡响不转）。
 *
 * 实测方法：烧录后看 1 档能否稳定启动。
 *   启动不了 → LEVEL1 每次 +50 往上调
 *   三档转速差别不明显 → LEVEL1 调低、LEVEL3 保持 950
 * 三档之间建议至少差 200，否则演示时观众看不出区别。 */
#define MOTOR_DUTY_LEVEL1    550u
#define MOTOR_DUTY_LEVEL2    750u
#define MOTOR_DUTY_LEVEL3    950u
#define MOTOR_DUTY_MAX       MOTOR_PWM_ARR

/* 渐变步长：每 10ms 变化多少。
 * 20 → 从 0 升到 1000 约需 500ms，平顺、电流冲击小、演示效果好 */
#define MOTOR_RAMP_STEP      20u

/* ================= 对外接口 ================= */

void     Motor_Init(void);                /* 初始化 GPIO 与 TIM3 PWM 输出 */
void     Motor_SetDuty(uint16_t duty);    /* 立即设定占空比（不渐变） */
void     Motor_RampTo(uint16_t target);   /* 设定目标值，由 RampUpdate 渐变过去 */
void     Motor_RampUpdate(void);          /* ★每 10ms 调用一次★，执行渐变 */
uint16_t Motor_GetDuty(void);             /* 读当前实际占空比 */
uint16_t Motor_GetTarget(void);           /* 读当前目标占空比 */

#endif /* __BSP_MOTOR_H */
