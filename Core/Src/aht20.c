/**
 * @file    aht20.c
 * @brief   AHT20/AHT21/AHT30 温湿度传感器 I2C 驱动
 *
 * 通信协议:
 *   1. 初始化: 发 0xBE, 读状态, 若未校准则发 0xBE/0x08/0x00
 *   2. 触发测量: 发 0xAC/0x33/0x00, 等 80ms
 *   3. 读取 6 字节数据, 计算温湿度
 *
 * 温湿度计算公式:
 *   RH%  = (raw_humidity * 100.0) / 1048576.0
 *   T℃  = (raw_temperature * 200.0) / 1048576.0 - 50.0
 */

#include "aht20.h"

#define AHT20_TIMEOUT  100

/* ========================================================================== */
/* 内部函数                                                                    */
/* ========================================================================== */

/**
 * @brief  读取 AHT20 状态字节
 */
static HAL_StatusTypeDef AHT20_ReadStatus(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    return HAL_I2C_Master_Receive(hi2c, AHT20_I2C_ADDR << 1, status, 1, AHT20_TIMEOUT);
}

/* ========================================================================== */
/* API 实现                                                                    */
/* ========================================================================== */

HAL_StatusTypeDef AHT20_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t status;
    HAL_StatusTypeDef ret;

    /* 等待传感器上电稳定 */
    HAL_Delay(45);

    /* 读状态, 检查是否需要校准 */
    ret = AHT20_ReadStatus(hi2c, &status);
    if (ret != HAL_OK) return ret;

    /* bit3 = 1 表示已校准, 无需重复初始化 */
    if (status & 0x08) return HAL_OK;

    /* 发送校准命令 */
    uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};
    ret = HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDR << 1, init_cmd, 3, AHT20_TIMEOUT);
    if (ret != HAL_OK) return ret;

    HAL_Delay(10);

    /* 检查校准结果 */
    ret = AHT20_ReadStatus(hi2c, &status);
    if (ret != HAL_OK) return ret;

    if (!(status & 0x08)) return HAL_ERROR; /* 校准失败 */

    return HAL_OK;
}

HAL_StatusTypeDef AHT20_Read(I2C_HandleTypeDef *hi2c, AHT20_Data_t *data)
{
    uint8_t buf[6];
    HAL_StatusTypeDef ret;
    uint32_t raw_hum, raw_temp;

    if (data == NULL) return HAL_ERROR;

    /* 1. 触发测量 */
    uint8_t trig_cmd[3] = {0xAC, 0x33, 0x00};
    ret = HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDR << 1, trig_cmd, 3, AHT20_TIMEOUT);
    if (ret != HAL_OK) {
        data->valid = 0;
        return ret;
    }

    /* 2. 等待测量完成 (80ms) */
    HAL_Delay(80);

    /* 3. 读取 6 字节数据 */
    ret = HAL_I2C_Master_Receive(hi2c, AHT20_I2C_ADDR << 1, buf, 6, AHT20_TIMEOUT);
    if (ret != HAL_OK) {
        data->valid = 0;
        return ret;
    }

    /* 4. 解析数据
     *   buf[0]: 状态 (bit7=busy)
     *   buf[1]: Humidity[19:12]
     *   buf[2]: Humidity[11:4]
     *   buf[3]: Humidity[3:0] | Temperature[19:16]
     *   buf[4]: Temperature[15:8]
     *   buf[5]: Temperature[7:0]
     */
    if (buf[0] & 0x80) {
        data->valid = 0;
        return HAL_BUSY;
    }

    raw_hum  = ((uint32_t)buf[1] << 12)
             | ((uint32_t)buf[2] << 4)
             | ((uint32_t)buf[3] >> 4);

    raw_temp = (((uint32_t)buf[3] & 0x0F) << 16)
             | ((uint32_t)buf[4] << 8)
             |  (uint32_t)buf[5];

    data->humidity    = (float)raw_hum  * 100.0f / 1048576.0f;
    data->temperature = (float)raw_temp * 200.0f / 1048576.0f - 50.0f;
    data->valid = 1;

    return HAL_OK;
}
