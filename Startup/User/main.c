
/**
 ******************************************************************************
 * @file    main.c
 * @brief   可调档位智能风扇 —— 主程序
 *          沁恒 CH32V307VCT6 ｜ 第一届新生嵌入式芯片应用创意赛（沁恒赛道）
 *
 * ==================== 程序结构（超级循环 Super-Loop） ====================
 *
 *   上电 → 初始化（时钟/串口/LED/按键/电机PWM/ADC）
 *        → 打印开机信息
 *        → while(1) 每 10ms 一圈：
 *             ① 扫按键（消抖 + 状态机改档位）
 *             ② 电机占空比渐变一步
 *             ③ 每 500ms 采一次温度，自动模式下重算档位
 *             ④ 每 2s 打印一次状态（调试用）
 *
 *   为什么用"主循环 + 10ms 延时"而不是定时器中断？
 *   对本项目（按键、调速、显示）完全够用，而且结构最简单、最好调试。
 *   零基础阶段先保证"能稳定跑通"，不要一上来就搞中断和 RTOS。
 *   以后要加联网等复杂功能，再改成"定时器中断置标志位、主循环处理"。
 *
 * ==================== 按键说明 ====================
 *   POWER (PB0) ：开 / 关
 *   UP    (PB1) ：档位 +1（手动模式下有效，3 档封顶）
 *   DOWN  (PB12)：档位 −1（手动模式下有效，1 档封底）
 *   MODE  (PB13)：手动 ↔ 自动 切换（自动档按温度调速）
 ******************************************************************************
 */
#include "debug.h"      /* 官方：Delay_Init / Delay_Ms / Delay_Us / printf 重定向 */
#include "bsp_config.h"
#include "bsp_motor.h"
#include "bsp_key.h"
#include "bsp_temp.h"
#include "bsp_led.h"
#include "app_fan.h"
#include "app_print.h"
#include "oled.h" 

/* 串口打印周期：200 个 tick × 10ms = 2s */
#define PRINT_PERIOD_TICK   200u

static uint16_t s_print_tick = 0;

int main(void)
{
    /* ---- 1. 系统与串口初始化（官方 debug.c）----
     * Delay_Init() 会配置 SysTick，必须先调用，否则后面所有延时都不准 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Delay_Init();
    USART_Printf_Init(DEBUG_BAUDRATE);

    printf("\r\n");
    printf("======================================\r\n");
    printf("  Smart Fan  CH32V307  v1.0\r\n");
    printf("  PWM=PA6(TIM3_CH1)  ADC=PA1(LM35)\r\n");
    printf("  KEY: PB0=POWER PB1=UP PB12=DOWN PB13=MODE\r\n");
    printf("======================================\r\n");

    /* ---- 2. 外设初始化 ----
     * 顺序有讲究：先 LED（能立刻看到反馈），再按键、电机、温度 */
    Led_Init();
    Key_Init();
    Motor_Init();
    Temp_Init();
    Fan_Init();

    printf("[SYS] init done, PWM freq=%dHz ARR=%d\r\n",
           (int)(MOTOR_TIM_CLK_HZ / (MOTOR_TIM_PSC + 1u) / (MOTOR_PWM_ARR + 1u)),
           (int)MOTOR_PWM_ARR);
    printf("[SYS] standby, press POWER to start\r\n");

    /* 开机自检：四个 LED 依次亮一下，确认接线和 LED_ACTIVE_LEVEL 正确
     * ★如果这一步灯不亮或逻辑反了，先去 bsp_config.h 改 LED_ACTIVE_LEVEL★ */
    {
        uint8_t i;
        for (i = 0; i < 4u; i++) {
            Led_Show((i < 3u) ? (i + 1u) : 0u, (i == 3u) ? 1u : 0u);
            Delay_Ms(200);
        }
        Led_AllOff();
    }   

   
    {
        GPIO_InitTypeDef g = {0};
        g.GPIO_Pin   = GPIO_Pin_6;
        g.GPIO_Mode  = GPIO_Mode_Out_PP;
        g.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOB, &g);
    }

   


    /* ---- 3. 主循环，每 10ms 一圈 ---- */
    while (1) {
        Fan_Task();          /* 按键 + 状态机 + 电机渐变 + 温度闭环 */

        /* 定期打印状态，方便用串口助手观察，答辩演示也能用上 */
        s_print_tick++;
        if (s_print_tick >= PRINT_PERIOD_TICK) {
            s_print_tick = 0;
            printf("[STAT] pwr=%d mode=%s lvl=%d temp=%d.%dC raw=%d duty=%d/%d\r\n",
                   Fan_GetPower(),
                   (Fan_GetMode() == FAN_MODE_AUTO) ? "AUTO" : "MANUAL",
                   Fan_GetLevel(),
                   PRINT_INT(Temp_Get()), PRINT_FRAC(Temp_Get()),
                   Temp_GetRaw(),
                   Motor_GetDuty(),
                   Motor_GetTarget());
        }

        Delay_Ms(TICK_MS);   /* 节拍。放在最后，保证一圈约 10ms */
    }
}
