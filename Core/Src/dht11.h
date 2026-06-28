/**
 * @file    dht11.h
 * @brief   DHT11 温湿度传感器 单总线驱动
 * @note    数据线: PA1, 上拉 4.7kΩ~10kΩ 至 VDD
 *
 *          通信协议 (单总线, 非 I2C):
 *            1. MCU 拉低 ≥18ms 作为起始信号
 *            2. MCU 释放总线, 等待 DHT11 响应 (80µs 低 + 80µs 高)
 *            3. DHT11 发送 40 位数据 (5 字节)
 *            4. 校验和 = 前 4 字节求和取低 8 位
 *
 *          数据格式:
 *            Byte0: 湿度整数    (DHT11 湿度小数恒为零)
 *            Byte1: 湿度小数
 *            Byte2: 温度整数
 *            Byte3: 温度小数    (DHT11 温度小数恒为零)
 *            Byte4: 校验和
 */

#ifndef __DHT11_H__
#define __DHT11_H__

#include "stm32c0xx_hal.h"

/* 温湿度数据结构体 (与 AHT20_Data_t 兼容) */
typedef struct {
    float temperature;   /* 温度 (℃) */
    float humidity;      /* 湿度 (%) */
    uint8_t valid;       /* 数据有效标志 */
} DHT11_Data_t;

/**
 * @brief  初始化 DHT11 (校准延时, 使能微秒定时器)
 * @retval HAL_OK
 */
HAL_StatusTypeDef DHT11_Init(void);

/**
 * @brief  读取温湿度
 * @param  data: 输出数据指针
 * @retval HAL_OK = 成功, HAL_TIMEOUT = 超时, HAL_ERROR = 校验失败
 */
HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *data);

#endif /* __DHT11_H__ */
