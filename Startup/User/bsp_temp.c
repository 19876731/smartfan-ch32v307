/**
 ******************************************************************************
 * @file    bsp_temp.c
 * @brief   LM35 温度采集实现（ADC1 + 中值滤波）
 * @note    例程参考：EVT\EXAM\ADC\ADC1
 *
 * 【换算公式，答辩会问】
 *   LM35 输出 10mV/℃，ADC 是 12 位、参考电压 3.3V
 *   → 电压(mV) = ADC值 × 3300 / 4096
 *   → 温度(℃) = 电压(mV) / 10 = ADC值 × 3300 / 4096 / 10
 *
 *   举例：室温 25℃ 时 LM35 输出 250mV
 *        ADC值 = 250 / 3300 × 4096 ≈ 310
 *        反算 = 310 × 3300 / 4096 / 10 ≈ 25.0℃  ✓
 *
 * 【分辨率】3300mV / 4096 ≈ 0.81mV，对应 0.081℃，精度远够用
 ******************************************************************************
 */
#include "bsp_temp.h"
#include "bsp_config.h"
#include "debug.h"      /* 官方 Delay_Us() */

static float    s_temp = 25.0f;
static uint16_t s_raw  = 0;

/**
 * @brief  ADC1 初始化，单通道、单次转换、软件触发
 */
void Temp_Init(void)
{
    ADC_InitTypeDef  ADC_InitStructure  = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* GPIO 在 APB2，ADC1 也在 APB2 —— 一次打开两个时钟 */
    RCC_APB2PeriphClockCmd(TEMP_ADC_RCC | RCC_APB2Periph_ADC1, ENABLE);

    /* ADC 时钟 = PCLK2 / 8 = 144M/8 = 18MHz
     * ⚠️ ADC 最高只能跑 14MHz，官方例程统一用 Div8，照抄即可。
     *    若读数跳动大，可改成 RCC_PCLK2_Div16 */
    RCC_ADCCLKConfig(TEMP_ADC_CLK_DIV);

    /* 温度引脚配成"模拟输入"AIN —— 不是浮空输入，也不是上拉输入 */
    GPIO_InitStructure.GPIO_Pin  = TEMP_ADC_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(TEMP_ADC_PORT, &GPIO_InitStructure);

    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;   /* 只用 ADC1，不与 ADC2 联动 */
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;                /* 单通道，不用扫描 */
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;                /* 单次转换，手动触发 */
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None; /* 软件触发 */
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;    /* 右对齐 */
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(TEMP_ADC_UNIT, &ADC_InitStructure);

    /* 使能 ADC，然后做校准 —— ★校准这两步别省★
     * 不校准的话读数会有明显偏差，这是 ADC 新手最常见的"读数不准"原因 */
    ADC_Cmd(TEMP_ADC_UNIT, ENABLE);

    ADC_ResetCalibration(TEMP_ADC_UNIT);
    while (ADC_GetResetCalibrationStatus(TEMP_ADC_UNIT));
    ADC_StartCalibration(TEMP_ADC_UNIT);
    while (ADC_GetCalibrationStatus(TEMP_ADC_UNIT));

    /* 稳定一下 */
    Delay_Ms(10);
}

/**
 * @brief  做一次 ADC 转换，返回 12 位原始值
 */
static uint16_t Temp_ReadRawOnce(void)
{
    /* 选择通道和采样时间。采样时间越长越稳定，这里用最长的 239.5 周期 */
    ADC_RegularChannelConfig(TEMP_ADC_UNIT, TEMP_ADC_CHANNEL, 1,
                             ADC_SampleTime_239Cycles5);

    ADC_SoftwareStartConvCmd(TEMP_ADC_UNIT, ENABLE);

    /* 等转换完成。加了超时保护，万一 EOC 一直不来也不会把程序卡死在这里 */
    {
        uint32_t timeout = 100000u;
        while (!ADC_GetFlagStatus(TEMP_ADC_UNIT, ADC_FLAG_EOC)) {
            if (--timeout == 0u) { break; }
        }
    }
    ADC_ClearFlag(TEMP_ADC_UNIT, ADC_FLAG_EOC);

    /* 读走转换结果。不关闭软件触发（不调 DISABLE），与官方例程保持一致：
     * 下次再调 ADC_SoftwareStartConvCmd(ENABLE) 就会重新触发一次转换 */
    return ADC_GetConversionValue(TEMP_ADC_UNIT);
}

/**
 * @brief  5 个数取中值（插入排序后取第 3 个）
 * @note   中值滤波比平均值更能抵抗偶发毛刺：
 *         一个离谱的读数不会影响结果，而平均值会被拉偏。
 */
static uint16_t Median5(uint16_t *a)
{
    uint8_t i, j;
    uint16_t tmp;

    for (i = 1; i < 5u; i++) {
        tmp = a[i];
        j = i;
        while ((j > 0u) && (a[j - 1u] > tmp)) {
            a[j] = a[j - 1u];
            j--;
        }
        a[j] = tmp;
    }
    return a[2];
}

float Temp_Update(void)
{
    uint16_t buf[5];
    uint8_t  i;

    for (i = 0; i < 5u; i++) {
        buf[i] = Temp_ReadRawOnce();
        Delay_Us(200);      /* 每次采样间隔 200us，让 ADC 输入电容充分充放电 */
    }

    s_raw  = Median5(buf);
    s_temp = (float)s_raw * TEMP_VREF_MV / TEMP_ADC_FULLSCALE / TEMP_MV_PER_DEGREE;

    /* 合理性限幅：LM35 量程 0~100℃，超出的读数一定是干扰或没接好 */
    if (s_temp < 0.0f)   { s_temp = 0.0f;   }
    if (s_temp > 100.0f) { s_temp = 100.0f; }

    return s_temp;
}

float Temp_Get(void)
{
    return s_temp;
}

uint16_t Temp_GetRaw(void)
{
    return s_raw;
}
