/**
 * @file    aht20.h
 * @brief   AHT20/AHT21/AHT30 温湿度传感器 I2C 驱动
 * @note    I2C 地址 0x38, 接口: I2C1 (PB7/SCL, PC14/SDA)
 */

#ifndef __AHT20_H__
#define __AHT20_H__

#include "stm32c0xx_hal.h"

/* AHT20 7-bit I2C 地址 */
#define AHT20_I2C_ADDR    0x38

/* 温湿度数据结构体 */
typedef struct {
    float temperature;   /* 温度 (℃) */
    float humidity;      /* 湿度 (%) */
    uint8_t valid;       /* 数据有效标志 */
} AHT20_Data_t;

/**
 * @brief  初始化 AHT20 传感器 (校准)
 * @param  hi2c: I2C 句柄指针
 * @retval HAL_OK = 成功, 其他 = 失败
 */
HAL_StatusTypeDef AHT20_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  读取温湿度
 * @param  hi2c: I2C 句柄指针
 * @param  data: 输出数据指针
 * @retval HAL_OK = 成功, 其他 = 失败
 */
HAL_StatusTypeDef AHT20_Read(I2C_HandleTypeDef *hi2c, AHT20_Data_t *data);

#endif /* __AHT20_H__ */
