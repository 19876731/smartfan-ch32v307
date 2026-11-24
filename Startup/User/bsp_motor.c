/**
 ******************************************************************************
 * @file    bsp_motor.c
 * @brief   直流电机 PWM 调速驱动实现
 * @note    基于 CH32V30x 官方标准外设库（EVT\EXAM\SRC\Peripheral）
 *          例程参考：EVT\EXAM\TIM\PWM_Output
 ******************************************************************************
 */
#include "bsp_motor.h"
#include "bsp_config.h"

/* 当前实际输出的占空比（渐变过程中会变化） */
static uint16_t s_duty_now    = 0;
/* 目标占空比 */
static uint16_t s_duty_target = 0;

/**
 * @brief  初始化电机 PWM
 * @note   PA6 复用为 TIM3_CH1，输出 PWM1 模式
 *         PWM1 模式：计数值 < CCR 时输出高电平，所以 CCR 越大占空比越大
 */
void Motor_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef       TIM_OCInitStructure = {0};

    /* 1. 打开时钟：GPIO 在 APB2，TIM3 在 APB1 —— 这两个别搞混 */
    RCC_APB2PeriphClockCmd(MOTOR_PWM_GPIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(MOTOR_PWM_TIM_RCC, ENABLE);

    /* 2. 配置 PWM 引脚为"复用推挽输出"
     *    注意必须是 GPIO_Mode_AF_PP（AF = Alternate Function 复用功能），
     *    写成 GPIO_Mode_Out_PP 的话引脚只是普通输出，定时器信号出不来 */
    GPIO_InitStructure.GPIO_Pin   = MOTOR_PWM_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MOTOR_PWM_GPIO_PORT, &GPIO_InitStructure);

    /* 3. 定时器时基：决定 PWM 频率
     *    频率 = TIM_CLK / (PSC+1) / (ARR+1) = 96M / 96 / 1000 = 1kHz */
    TIM_TimeBaseStructure.TIM_Period        = MOTOR_PWM_ARR;
    TIM_TimeBaseStructure.TIM_Prescaler     = MOTOR_TIM_PSC;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(MOTOR_PWM_TIM, &TIM_TimeBaseStructure);

    /* 4. 输出比较通道 1，PWM1 模式，初始占空比 0 */
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(MOTOR_PWM_TIM, &TIM_OCInitStructure);

    /* 5. 打开预装载：改 CCR 值在下个周期才生效，避免输出毛刺
     *    这一步是"切档平顺"的关键，别省 */
    TIM_OC1PreloadConfig(MOTOR_PWM_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(MOTOR_PWM_TIM, ENABLE);

    /* 6. 启动定时器 */
    TIM_Cmd(MOTOR_PWM_TIM, ENABLE);

    s_duty_now    = 0;
    s_duty_target = 0;

    /* TIM3 是通用定时器，不需要 TIM_CtrlPWMOutputs()
     * （只有 TIM1/TIM8 这类高级定时器才需要，这是新手最常踩的坑） */
}

/**
 * @brief  立即设定占空比（不渐变）
 * @param  duty: 0 ~ MOTOR_PWM_ARR
 */
void Motor_SetDuty(uint16_t duty)
{
    if (duty > MOTOR_DUTY_MAX) {
        duty = MOTOR_DUTY_MAX;      /* 防止越界写坏寄存器 */
    }
    s_duty_now    = duty;
    s_duty_target = duty;
    TIM_SetCompare1(MOTOR_PWM_TIM, s_duty_now);
}

/**
 * @brief  设定目标占空比，由 Motor_RampUpdate() 渐变过渡
 * @param  target: 0 ~ MOTOR_PWM_ARR
 */
void Motor_RampTo(uint16_t target)
{
    if (target > MOTOR_DUTY_MAX) {
        target = MOTOR_DUTY_MAX;
    }
    s_duty_target = target;
}

/**
 * @brief  渐变执行一步
 * @note   ★必须在主循环里每 10ms 调用一次★
 *         作用：让占空比一步步靠近目标值，而不是一下子跳过去。
 *         好处：① 电机启动电流冲击小，不容易把电压拉低导致单片机复位
 *              ② 转速平滑变化，演示和拍视频效果明显更好
 */
void Motor_RampUpdate(void)
{
    if (s_duty_now < s_duty_target) {
        uint16_t diff = s_duty_target - s_duty_now;
        s_duty_now += (diff > MOTOR_RAMP_STEP) ? MOTOR_RAMP_STEP : diff;
    } else if (s_duty_now > s_duty_target) {
        uint16_t diff = s_duty_now - s_duty_target;
        s_duty_now -= (diff > MOTOR_RAMP_STEP) ? MOTOR_RAMP_STEP : diff;
    }

    TIM_SetCompare1(MOTOR_PWM_TIM, s_duty_now);
}

uint16_t Motor_GetDuty(void)
{
    return s_duty_now;
}

uint16_t Motor_GetTarget(void)
{
    return s_duty_target;
}
