/**
 * @file    lcd_st7032s.c
 * @brief   ST7032S 字符型 LCD 驱动 (适配 晶联讯 JLX1602G-916)
 *
 * 严格按 晶联讯 JLX1602G-916 说明书 v2019-02-20 上的官方初始化程序写:
 *   - 4线 SPI 时序 = SPI Mode 0 (CPOL=0, CPHA=0)
 *     参考代码: sclk=0 时设数据, sclk=1 时采样 → CPOL=0 + CPHA=0
 *   - 初始化序列 (厂家提供):
 *       0x38, 0x01, delay(5)
 *       0x06, delay(5)
 *       0x0c, delay(5)
 *       0x39, 0x1c, delay(5)     <- extension instruction + bias/OSC
 *       0x6d, delay(5)            <- follower on
 *       0x55, delay(5)            <- power/ICON/contrast 粗调
 *       0x7a, delay(5)            <- contrast 微调 = 10
 *   - 写 CGRAM/DDRAM 前必须切回 0x38 (normal instruction)
 *
 * 软件 CS (PA4) + RS (PA6) + SPI, RST/BLK 由外部电路处理
 */

#include "lcd_st7032s.h"
#include <stddef.h>

/* SPI 句柄在 main.c 里由 CubeMX 生成 */
extern SPI_HandleTypeDef LCD_SPI_HANDLE;

/* ========================================================================== */
/* 内部函数                                                                    */
/* ========================================================================== */

static void LCD_DelayUs(uint32_t us)
{
    /* 48MHz 下大约 12 cycles/us */
    us *= 12;
    while (us--) {
        __NOP();
    }
}

/**
 * @brief  写指令 (RS=0, 按说明书 rs=0 为指令寄存器)
 */
static void LCD_WriteCmd(uint8_t cmd)
{
    LCD_RS_LOW();   /* RS=0: 指令寄存器 */
    LCD_CS_LOW();
    HAL_SPI_Transmit(&LCD_SPI_HANDLE, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

/**
 * @brief  写数据 (RS=1, 按说明书 rs=1 为数据寄存器)
 */
static void LCD_WriteData(uint8_t data)
{
    LCD_RS_HIGH();  /* RS=1: 数据寄存器 */
    LCD_CS_LOW();
    HAL_SPI_Transmit(&LCD_SPI_HANDLE, &data, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

/* ========================================================================== */
/* API 实现                                                                    */
/* ========================================================================== */

void LCD_Init(void)
{
    /*
     * 严格按 JLX1602G-916 说明书 v2019-02-20 上的官方初始化程序:
     */
    LCD_WriteCmd(LCD_CMD_FUNCTION_SET);          /* function select (normal) */
    LCD_WriteCmd(LCD_CMD_CLEAR);                /* clear screen */
    HAL_Delay(5);
    LCD_WriteCmd(LCD_CMD_ENTRY_MODE);           /* entry mode set */
    HAL_Delay(5);
    LCD_WriteCmd(LCD_CMD_DISPLAY_ON);           /* display on */
    HAL_Delay(5);

    /* 切到扩展指令集 */
    LCD_WriteCmd(LCD_CMD_FUNCTION_SET_EXT);     /* extension instruction */
    LCD_WriteCmd(LCD_CMD_BIAS_OSC);             /* bias=1/4, OSC freq (BS=1, FX=1, F1=1, F0=0) */
    HAL_Delay(5);
    LCD_WriteCmd(LCD_CMD_FOLLOWER_ON);          /* follower on (Fon=1, Rab2..0=101) */
    HAL_Delay(5);
    LCD_WriteCmd(LCD_CMD_POWER_ICON);           /* power/ICON/contrast 粗调 (Bon=1, Ion=0) */
    HAL_Delay(5);
    LCD_WriteCmd(LCD_CMD_CONTRAST_BASE | 0x0A); /* contrast 微调 = 10 */
    HAL_Delay(5);

    /* 切回 normal, 为后续 CGRAM/DDRAM 操作做准备 */
    LCD_WriteCmd(LCD_CMD_FUNCTION_SET);
}

void LCD_Clear(void)
{
    LCD_WriteCmd(LCD_CMD_CLEAR);
    HAL_Delay(3);
}

void LCD_Home(void)
{
    LCD_WriteCmd(LCD_CMD_HOME);
    HAL_Delay(3);
}

void LCD_SetCursor(uint8_t col, uint8_t row)
{
    uint8_t addr = (row == 0) ? (0x00 + col) : (0x40 + col);
    LCD_WriteCmd(LCD_CMD_SET_DDRAM_ADDR | addr);
    HAL_Delay(1);
}

void LCD_PutChar(char c)
{
    LCD_WriteData((uint8_t)c);
    LCD_DelayUs(50);
}

void LCD_Print(uint8_t col, uint8_t row, const char *str)
{
    if (str == NULL) return;
    LCD_SetCursor(col, row);
    while (*str) {
        LCD_PutChar(*str++);
    }
}

void LCD_PrintNum(uint8_t col, uint8_t row, int32_t num, uint8_t width)
{
    char buf[12];
    int8_t i = 0;
    int8_t neg = 0;
    int8_t start, end;

    if (num < 0) {
        neg = 1;
        num = -num;
    }
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0) {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    if (neg) buf[i++] = '-';
    while (i < width) buf[i++] = ' ';

    /* 反转 */
    start = 0;
    end = i - 1;
    while (start < end) {
        char t = buf[start];
        buf[start] = buf[end];
        buf[end] = t;
        start++;
        end--;
    }
    buf[i] = '\0';

    LCD_Print(col, row, buf);
}

void LCD_DisplayCtrl(uint8_t on, uint8_t cursor, uint8_t blink)
{
    uint8_t cmd = 0x08;
    if (on)     cmd |= 0x04;
    if (cursor) cmd |= 0x02;
    if (blink)  cmd |= 0x01;
    LCD_WriteCmd(cmd);
    HAL_Delay(1);
}

/**
 * @brief  创建自定义字符 (5x8 点阵)
 * @note   必须在 normal instruction 模式下调用
 */
void LCD_CreateChar(uint8_t index, const uint8_t data[8])
{
    uint8_t i;
    if (index > 7) return;

    /* 确保在 normal 模式 */
    LCD_WriteCmd(LCD_CMD_FUNCTION_SET);
    HAL_Delay(1);

    /* 设置 CGRAM 地址 */
    LCD_WriteCmd(LCD_CMD_SET_CGRAM_ADDR | (index << 3));
    HAL_Delay(1);
    for (i = 0; i < 8; i++) {
        LCD_WriteData(data[i]);
        LCD_DelayUs(50);
    }

    /* 切回 DDRAM 0 地址, 避免影响后续 print */
    LCD_WriteCmd(LCD_CMD_SET_DDRAM_ADDR);
    HAL_Delay(1);
}

void LCD_ShowCustomChar(uint8_t col, uint8_t row, uint8_t index)
{
    LCD_SetCursor(col, row);
    LCD_WriteData(index);
    LCD_DelayUs(50);
}

/**
 * @brief  软件调对比度 (0~15)
 * @note   必须在 extension instruction 模式下调用
 */
void LCD_SetContrast(uint8_t contrast)
{
    if (contrast > 15) contrast = 15;

    LCD_WriteCmd(LCD_CMD_FUNCTION_SET_EXT);       /* extension */
    HAL_Delay(1);
    LCD_WriteCmd(LCD_CMD_CONTRAST_BASE | contrast); /* contrast set */
    HAL_Delay(1);
    LCD_WriteCmd(LCD_CMD_FUNCTION_SET);            /* 切回 normal */
    HAL_Delay(1);
}

void LCD_Backlight(uint8_t on)
{
    (void)on;
    /* 纯 SPI 版本: 背光由外部电路控制, 此处为空实现 */
}
