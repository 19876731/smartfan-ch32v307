/**
 ******************************************************************************
 * @file    app_print.h
 * @brief   串口打印小工具
 *
 * 【为什么需要它 —— 一个很隐蔽的坑】
 *   MounRiver 默认用的 newlib-nano 精简版 printf，
 *   ★不支持 %.1f 这类浮点格式★，除非在编译选项里加 -u _printf_float。
 *   表现是：程序正常运行，但串口里温度那段是空白或乱码。
 *   很多新手会以为"ADC 没读到数据"，其实是 printf 打不出来。
 *
 *   解决办法（本项目采用）：把浮点数拆成整数部分 + 一位小数，用 %d 打印。
 *   不依赖任何编译选项，最稳。
 ******************************************************************************
 */
#ifndef __APP_PRINT_H
#define __APP_PRINT_H

#include "debug.h"

/**
 * @brief  把浮点数打印成 "整数.一位小数" 两段
 * @note   用法：printf("temp=%d.%dC", PRINT_INT(t), PRINT_FRAC(t));
 */
#define PRINT_INT(x)   ((int)(x))
#define PRINT_FRAC(x)  ((int)(((x) - (float)(int)(x)) * 10.0f + 0.5f))

#endif /* __APP_PRINT_H */
