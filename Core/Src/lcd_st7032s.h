/**
 * @file    lcd_st7032s.h
 * @brief   ST7032S 字符型 LCD 驱动 (适配 晶联讯 JLX1602G-916 等 16x2 COG 屏)
 * @note    SPI + 软件 CS
 *
 *          引脚分配:
 *            PA5  -> SPI1_SCK
 *            PA7  -> SPI1_MOSI
 *            PA4  -> LCD CS     (软件 GPIO 控制)
 *            PA6  -> LCD RS     (H:数据寄存器, L:指令寄存器/CD)
 *
 *          ST7032S 4线 SPI = SPI Mode 0 (CPOL=0, CPHA=0)
 *          参考 JLX1602G-916 说明书: sclk=0 时设数据, sclk=1 时采样
 */

#ifndef __LCD_ST7032S_H__
#define __LCD_ST7032S_H__

#include "stm32c0xx_hal.h"

/* SPI1 句柄 - CubeMX 生成 hspi1 */
#define LCD_SPI_HANDLE        hspi1

/* CS - PA4, 软件控制 */
#define LCD_CS_PORT           GPIOA
#define LCD_CS_PIN            GPIO_PIN_4
#define LCD_CS_HIGH()         HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define LCD_CS_LOW()          HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)

/* RS - PA6, H:数据寄存器, L:指令寄存器 (IC 标为 CD) */
#define LCD_RS_PORT           GPIOA
#define LCD_RS_PIN            GPIO_PIN_6
#define LCD_RS_HIGH()         HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_SET)
#define LCD_RS_LOW()          HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET)

/* ========================================================================== */
/* ST7032S 指令集 (参照 JLX1602G-916 说明书)                                   */
/* ========================================================================== */
#define LCD_CMD_CLEAR             0x01
#define LCD_CMD_HOME              0x02
#define LCD_CMD_ENTRY_MODE        0x06
#define LCD_CMD_DISPLAY_ON        0x0C
#define LCD_CMD_FUNCTION_SET      0x38
#define LCD_CMD_FUNCTION_SET_EXT  0x39
#define LCD_CMD_BIAS_OSC          0x1C  /* BS=1(1/4), FX=1, F1=1, F0=0 */
#define LCD_CMD_CONTRAST_BASE     0x70  /* 微调对比度基址 (C3..C0) */
#define LCD_CMD_POWER_ICON        0x55  /* Bon=1, Ion=0 */
#define LCD_CMD_FOLLOWER_ON       0x6D  /* Fon=1, Rab2..0=101 */
#define LCD_CMD_SET_DDRAM_ADDR    0x80
#define LCD_CMD_SET_CGRAM_ADDR    0x40

/* ========================================================================== */
/* API                                                                         */
/* ========================================================================== */

void LCD_Init(void);
void LCD_Clear(void);
void LCD_Home(void);
void LCD_SetCursor(uint8_t col, uint8_t row);
void LCD_PutChar(char c);
void LCD_Print(uint8_t col, uint8_t row, const char *str);
void LCD_PrintNum(uint8_t col, uint8_t row, int32_t num, uint8_t width);
void LCD_DisplayCtrl(uint8_t on, uint8_t cursor, uint8_t blink);
void LCD_CreateChar(uint8_t index, const uint8_t data[8]);
void LCD_ShowCustomChar(uint8_t col, uint8_t row, uint8_t index);
void LCD_SetContrast(uint8_t contrast);
void LCD_Backlight(uint8_t on);

#endif /* __LCD_ST7032S_H__ */
