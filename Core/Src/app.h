/**
 * @file    app.h
 * @brief   Application layer — sensors, keys, LED, LCD pages, main loop
 */

#ifndef __APP_H__
#define __APP_H__

#include "stm32c0xx_hal.h"

/* ---- 传感器选择 (二选一) ------------------------------------------------ */
#define USE_DHT11    /* 使用 DHT11 (PA1 单总线), 注释掉此行则使用 AHT20 (I2C) */
/* ------------------------------------------------------------------------ */

#ifdef USE_DHT11
#include "dht11.h"
#else
#include "aht20.h"
#endif

/* ========================================================================== */
/* CGRAM 自定义字符 (extern — 定义在 app.c)                                    */
/* ========================================================================== */
extern const uint8_t CGRAM_SMILE[8];
extern const uint8_t CGRAM_HEART[8];
extern const uint8_t CGRAM_DEGREE[8];
extern const uint8_t CGRAM_BSLASH[8];
extern const uint8_t CGRAM_PIPE[8];

/* ========================================================================== */
/* 五项按键                                                                     */
/* ========================================================================== */
typedef enum {
    KEY_NONE = 0,
    KEY_SELECT,     /* MRatio 0    → 0V    */
    KEY_LEFT,       /* MRatio 0.20 → 0.67V */
    KEY_DOWN,       /* MRatio 0.40 → 1.32V */
    KEY_UP,         /* MRatio 0.61 → 2.01V */
    KEY_RIGHT       /* MRatio 0.80 → 2.65V */
} KeyState_t;

#define PAGE_MAX    2

/* ========================================================================== */
/* 轻量格式化辅助 (pc_uart 共用)                                                */
/* ========================================================================== */
void fmt_2d(char *p, int8_t v);
void fmt_1d(char *p, int8_t v);

/* ========================================================================== */
/* 按键                                                                         */
/* ========================================================================== */
KeyState_t ADC_ReadKey(void);
const char* KeyName(KeyState_t key);

/* ========================================================================== */
/* LED                                                                          */
/* ========================================================================== */
void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);

/* ========================================================================== */
/* App API                                                                      */
/* ========================================================================== */
void APP_Init(void);
void APP_Loop(void);

#endif /* __APP_H__ */
