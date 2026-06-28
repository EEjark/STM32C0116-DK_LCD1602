/**
 * @file    pc_uart.h
 * @brief   UART1 PC 通信 — 硬件信息接收 / RTC 校准
 */

#ifndef __PC_UART_H__
#define __PC_UART_H__

#include "stm32c0xx_hal.h"

/* ========================================================================== */
/* PC 硬件信息结构体                                                             */
/* ========================================================================== */
typedef struct {
    int8_t  cpu_temp;     /* -1 = N/A */
    int8_t  gpu_temp;     /* -1 = N/A */
    int8_t  mem_usage;    /* -1 = N/A */
    char    uptime[12];
    uint8_t valid;
} PCInfo_t;

extern PCInfo_t g_PCInfo;
extern uint8_t  g_RTC_Synced;     /* 0=未校准, 1=已校准 */
extern uint32_t g_LastPcQuery;    /* 上次查询 PC 的秒数 */

/* ========================================================================== */
/* API                                                                          */
/* ========================================================================== */
void PC_UART_Init(void);
void PC_UART_Send(const char *s);
void PC_UART_ProcessRx(void);
void PC_UART_Query(void);         /* 发送 "ASK\n" */
void PC_UART_RequestTime(void);   /* 发送 "TIME\n" */

#endif /* __PC_UART_H__ */
