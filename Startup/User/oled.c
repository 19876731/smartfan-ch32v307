/* oled.c  --  0.96寸 OLED (SSD1306 128x64, I2C) 软件模拟 I2C 驱动
 *
 * 使用步骤：
 *   1. 把 oled.c / oled.h / oledfont.h 三个文件复制到工程 User 目录
 *   2. 在 main.c 里 #include "oled.h"
 *   3. 初始化里调用一次 OLED_Init()
 *   4. 主循环里调 OLED_ShowString / OLED_ShowInt 刷新
 *
 * 注意：本驱动只用推挽输出，不读取 SDA，因此不需要外部上拉也能工作
 *      （但模块若自带上拉更稳；通信不稳定时把 OLED_I2C_DELAY_N 调大）。
 */

#include "oled.h"

/* 字库数组定义在这里，避免多处包含造成重复定义 */
#include "oledfont.h"

/* ---------------- 底层 GPIO ---------------- */
#define OLED_SCL_H()   GPIO_SetBits(OLED_SCL_PORT,   OLED_SCL_PIN)
#define OLED_SCL_L()   GPIO_ResetBits(OLED_SCL_PORT, OLED_SCL_PIN)
#define OLED_SDA_H()   GPIO_SetBits(OLED_SDA_PORT,   OLED_SDA_PIN)
#define OLED_SDA_L()   GPIO_ResetBits(OLED_SDA_PORT, OLED_SDA_PIN)

static void OLED_Delay(void)
{
    volatile uint16_t i;
    for (i = 0; i < OLED_I2C_DELAY_N; i++);
}

static void OLED_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(OLED_GPIO_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;   /* 推挽输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(OLED_SCL_PORT, &GPIO_InitStructure);      /* SCL/SDA 同端口 */

    OLED_SCL_H();
    OLED_SDA_H();
}

/* ---------------- 软件 I2C ---------------- */
static void I2C_Start(void)
{
    OLED_SDA_H(); OLED_SCL_H(); OLED_Delay();
    OLED_SDA_L();               OLED_Delay();
    OLED_SCL_L();               OLED_Delay();
}

static void I2C_Stop(void)
{
    OLED_SCL_L(); OLED_SDA_L(); OLED_Delay();
    OLED_SCL_H();               OLED_Delay();
    OLED_SDA_H();               OLED_Delay();
}

static void I2C_SendByte(uint8_t byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80) { OLED_SDA_H(); } else { OLED_SDA_L(); }
        byte <<= 1;
        OLED_SCL_H(); OLED_Delay();
        OLED_SCL_L(); OLED_Delay();
    }
    /* 第 9 个时钟：忽略 ACK（SSD1306 只写不读，够用） */
    OLED_SDA_H();
    OLED_SCL_H(); OLED_Delay();
    OLED_SCL_L(); OLED_Delay();
}

static void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start();
    I2C_SendByte((uint8_t)(OLED_I2C_ADDR << 1));  /* 写地址 */
    I2C_SendByte(0x00);                          /* 控制字节：命令 */
    I2C_SendByte(cmd);
    I2C_Stop();
}

static void OLED_WriteData(uint8_t dat)
{
    I2C_Start();
    I2C_SendByte((uint8_t)(OLED_I2C_ADDR << 1));
    I2C_SendByte(0x40);                          /* 控制字节：数据 */
    I2C_SendByte(dat);
    I2C_Stop();
}

/* ---------------- 定位 / 清屏 ---------------- */
void OLED_SetPos(uint8_t x, uint8_t page)
{
    x += OLED_COL_OFFSET;
    OLED_WriteCmd(0xB0 + (page & 0x07));         /* 页地址 */
    OLED_WriteCmd(0x10 | ((x >> 4) & 0x0F));     /* 列地址高 4 位 */
    OLED_WriteCmd(0x00 | (x & 0x0F));            /* 列地址低 4 位 */
}

void OLED_Clear(void)
{
    uint8_t page, col;
    for (page = 0; page < 8; page++)
    {
        OLED_SetPos(0, page);
        for (col = 0; col < 128; col++)
        {
            OLED_WriteData(0x00);
        }
    }
}

/* ---------------- 显示 ---------------- */
/* 8x16 字符：上半 8 行放 page，下半 8 行放 page+1 */
void OLED_ShowChar(uint8_t x, uint8_t page, char ch)
{
    uint8_t i;
    uint8_t idx;

    if (ch < 32 || ch > 126) { ch = '?'; }
    idx = (uint8_t)(ch - 32);

    OLED_SetPos(x, page);
    for (i = 0; i < 8; i++)  { OLED_WriteData(OLED_Font8x16[idx][i]); }
    OLED_SetPos(x, page + 1);
    for (i = 8; i < 16; i++) { OLED_WriteData(OLED_Font8x16[idx][i]); }
}

void OLED_ShowString(uint8_t x, uint8_t page, const char *str)
{
    while (*str)
    {
        if (x > 120) { break; }              /* 超出屏幕宽度就停 */
        OLED_ShowChar(x, page, *str);
        x += 8;
        str++;
    }
}

void OLED_ShowInt(uint8_t x, uint8_t page, int32_t num)
{
    char buf[12];
    uint8_t pos = 0, i, j;
    uint8_t neg = 0;
    int32_t n = num;

    if (n < 0) { neg = 1; n = -n; }

    /* 先按低位在前写入 */
    if (n == 0) { buf[pos++] = '0'; }
    while (n > 0 && pos < 10)
    {
        buf[pos++] = (char)('0' + (n % 10));
        n /= 10;
    }

    /* 翻转成正序 */
    for (i = 0, j = pos; i < --j; i++)
    {
        char t = buf[i]; buf[i] = buf[j]; buf[j] = t;
    }

    /* 加负号 */
    if (neg)
    {
        for (i = pos; i > 0; i--) { buf[i] = buf[i - 1]; }
        buf[0] = '-';
        pos++;
    }
    buf[pos] = '\0';

    OLED_ShowString(x, page, buf);
}

/* ---------------- 初始化 ---------------- */
void OLED_Init(void)
{
    OLED_GPIO_Init();

    OLED_WriteCmd(0xAE);        /* 关显示 */

    OLED_WriteCmd(0x20);        /* 设置寻址模式 */
    OLED_WriteCmd(0x10);        /* 00=水平  01=垂直  10=页寻址 */

    OLED_WriteCmd(0xB0);        /* 页地址 0 */
    OLED_WriteCmd(0xC8);        /* COM 扫描方向（上下翻转改 0xC0） */
    OLED_WriteCmd(0x00);        /* 列低 4 位 */
    OLED_WriteCmd(0x10);        /* 列高 4 位 */
    OLED_WriteCmd(0x40);        /* 起始行 0 */

    OLED_WriteCmd(0x81);        /* 对比度 */
    OLED_WriteCmd(0xCF);

    OLED_WriteCmd(0xA1);        /* 段重映射（左右翻转改 0xA0） */
    OLED_WriteCmd(0xA6);        /* 正常显示（反显改 0xA7） */
    OLED_WriteCmd(0xA8);        /* 多路复用 */
    OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xA4);        /* 输出跟随 RAM */

    OLED_WriteCmd(0xD3);        /* 显示偏移 */
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0xD5);        /* 时钟分频 */
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xD9);        /* 预充电周期 */
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDA);        /* COM 引脚配置 */
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0xDB);        /* VCOMH */
    OLED_WriteCmd(0x30);

    OLED_WriteCmd(0x8D);        /* 电荷泵 */
    OLED_WriteCmd(0x14);        /* 必须开，否则屏不亮 */

    OLED_Clear();
    OLED_WriteCmd(0xAF);        /* 开显示 */
}
