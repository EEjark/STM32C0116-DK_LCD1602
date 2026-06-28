/**
 * @file    dht11.c
 * @brief   DHT11 温湿度传感器 单总线驱动实现
 * @note    使用 TIM1 产生 1MHz 时钟用于微秒延时
 *          数据线 PA1, 通过寄存器直接操作确保时序精度
 */

#include "dht11.h"

/* ========================================================================== */
/* 硬件定义 — DHT11 数据线: PA1                                               */
/* ========================================================================== */
#define DHT11_PORT          GPIOA
#define DHT11_PIN           GPIO_PIN_1
#define DHT11_PIN_NUM       1U

/* ========================================================================== */
/* 快速 GPIO 操作 (寄存器级, 避免 HAL 开销)                                    */
/* ========================================================================== */
#define DHT11_SET_OUTPUT()  do { \
    DHT11_PORT->MODER &= ~(3UL << (DHT11_PIN_NUM * 2)); \
    DHT11_PORT->MODER |=  (1UL << (DHT11_PIN_NUM * 2)); \
} while (0)

#define DHT11_SET_INPUT()   do { \
    DHT11_PORT->MODER &= ~(3UL << (DHT11_PIN_NUM * 2)); \
} while (0)

#define DHT11_WRITE_LOW()   do { \
    DHT11_PORT->BSRR = (uint32_t)DHT11_PIN << 16U; \
} while (0)

#define DHT11_WRITE_HIGH()  do { \
    DHT11_PORT->BSRR = DHT11_PIN; \
} while (0)

#define DHT11_READ_PIN()    ((DHT11_PORT->IDR & DHT11_PIN) ? 1U : 0U)

/* ========================================================================== */
/* TIM1 微秒延时                                                                */
/* ========================================================================== */
static uint8_t tim1_ready = 0;

static void TIM1_us_Init(void)
{
    if (tim1_ready) return;
    __HAL_RCC_TIM1_CLK_ENABLE();
    /* PSC = SysClock/MHz - 1  →  CNT 每 µs 加 1 */
    TIM1->PSC = (SystemCoreClock / 1000000U) - 1U;
    TIM1->ARR = 0xFFFF;
    TIM1->EGR = TIM_EGR_UG;     /* 立即更新 PSC */
    TIM1->CR1 = TIM_CR1_CEN;    /* 启动计数器 */
    tim1_ready = 1;
}

/**
 * @brief  微秒延时 (MAX 65535µs ≈ 65ms)
 */
static void delay_us(uint16_t us)
{
    uint16_t start = (uint16_t)TIM1->CNT;
    while ((uint16_t)(TIM1->CNT - start) < us) {
        __NOP();
    }
}

/* ========================================================================== */
/* DHT11 协议实现                                                               */
/* ========================================================================== */

HAL_StatusTypeDef DHT11_Init(void)
{
    TIM1_us_Init();

    /* 传感器上电稳定 (>1s) */
    HAL_Delay(1100);

    /* 总线空闲应为高电平 (外部上拉) */
    /* 若持续低电平则传感器未连接/损坏 */
    DHT11_SET_INPUT();
    for (volatile uint32_t i = 0; i < 100000; i++) {
        if (DHT11_READ_PIN()) break;
    }

    return HAL_OK;
}

HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *data)
{
    uint8_t  buf[5] = {0};
    uint32_t timeout;

    if (data == NULL) return HAL_ERROR;
    data->valid = 0;

    /* ---- 1. 主机起始信号: 拉低 ≥18ms, 然后释放 20~40µs ---- */
    DHT11_SET_OUTPUT();
    DHT11_WRITE_LOW();
    HAL_Delay(18);

    __disable_irq();  /* 关闭中断, 确保时序 */

    DHT11_WRITE_HIGH();
    delay_us(30);     /* 释放 30µs (±10µs 容差) */

    DHT11_SET_INPUT();

    /* ---- 2. 等待 DHT11 响应: 低 80µs + 高 80µs ---- */
    timeout = 200;
    while (DHT11_READ_PIN() == 0) {
        if (--timeout == 0) { __enable_irq(); return HAL_TIMEOUT; }
        delay_us(1);
    }

    timeout = 200;
    while (DHT11_READ_PIN() == 1) {
        if (--timeout == 0) { __enable_irq(); return HAL_TIMEOUT; }
        delay_us(1);
    }

    /* ---- 3. 读取 40 位数据 (5 字节) ---- */
    for (uint8_t byte = 0; byte < 5; byte++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            /* 等待该 bit 起始低电平 (50µs) 结束 */
            timeout = 120;
            while (DHT11_READ_PIN() == 0) {
                if (--timeout == 0) { __enable_irq(); return HAL_TIMEOUT; }
                delay_us(1);
            }

            /* 高电平持续 ~27µs = 0, ~70µs = 1
             * 延迟 40µs 后采样: 若仍高 → 1, 若已低 → 0 */
            delay_us(40);
            buf[byte] <<= 1;
            if (DHT11_READ_PIN()) {
                buf[byte] |= 1;
            }

            /* 等待高电平结束 (进入下一 bit 或结束) */
            timeout = 120;
            while (DHT11_READ_PIN() == 1) {
                if (--timeout == 0) { __enable_irq(); return HAL_TIMEOUT; }
                delay_us(1);
            }
        }
    }

    __enable_irq();

    /* ---- 4. 校验和 ---- */
    if (buf[4] != (uint8_t)(buf[0] + buf[1] + buf[2] + buf[3])) {
        return HAL_ERROR;
    }

    /* ---- 5. 解析 ---- */
    /* DHT11: 湿度/温度小数恒为 0, 仅取整数 */
    data->humidity    = (float)buf[0];
    data->temperature = (float)buf[2];
    data->valid = 1;

    return HAL_OK;
}
