/**
 * @file    pc_uart.c
 * @brief   UART1 PC 通信实现 — 接收 PC 硬件信息, 解析 TIME 校准 RTC
 */

#include "pc_uart.h"
#include "app.h"
#include <string.h>

/* ========================================================================== */
/* 外部 HAL 句柄                                                                */
/* ========================================================================== */
extern UART_HandleTypeDef huart1;
extern RTC_HandleTypeDef  hrtc;

/* ========================================================================== */
/* 模块内部状态                                                                  */
/* ========================================================================== */
static char    g_UART_RxBuf[64];
static uint8_t g_UART_RxIdx = 0;

/* 公共全局变量 */
PCInfo_t g_PCInfo = {0};
uint8_t  g_RTC_Synced = 0;
uint32_t g_LastPcQuery = 99;

/* ========================================================================== */
/* 轻量解析器                                                                    */
/* ========================================================================== */
static int8_t parse_i8(const char **s)
{
    int8_t v = 0, neg = 0;
    while (**s && (**s < '0' || **s > '9') && **s != '-') (*s)++;
    if (**s == '-') { neg = 1; (*s)++; }
    while (**s >= '0' && **s <= '9') { v = v * 10 + (**s - '0'); (*s)++; }
    return neg ? -v : v;
}

/* ========================================================================== */
/* RTC 校准                                                                      */
/* ========================================================================== */
static uint8_t RTC_CalcWeekDay(uint8_t y, uint8_t m, uint8_t d)
{
    static const int8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t year = 2000 + y;
    if (m < 3) year--;
    uint8_t w = (uint8_t)((year + year/4 - year/100 + year/400 + t[m-1] + d) % 7);
    return (w == 0) ? RTC_WEEKDAY_SUNDAY : w;
}

static uint8_t RTC_SetFromSerial(const char *s)
{
    if (s[0]!='T' || s[1]!='I' || s[2]!='M' || s[3]!='E' || s[4]!=':')
        return 0;
    s += 5;
    for (uint8_t i = 0; i < 12; i++)
        if (s[i] < '0' || s[i] > '9') return 0;

    int8_t yr = (s[0]-'0')*10 + (s[1]-'0');
    int8_t mo = (s[2]-'0')*10 + (s[3]-'0');
    int8_t dy = (s[4]-'0')*10 + (s[5]-'0');
    int8_t hr = (s[6]-'0')*10 + (s[7]-'0');
    int8_t mi = (s[8]-'0')*10 + (s[9]-'0');
    int8_t sc = (s[10]-'0')*10 + (s[11]-'0');

    if (mo < 1 || mo > 12 || dy < 1 || dy > 31 || hr > 23 || mi > 59 || sc > 59)
        return 0;

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    sTime.Hours   = hr;
    sTime.Minutes = mi;
    sTime.Seconds = sc;
    sDate.WeekDay = RTC_CalcWeekDay(yr, mo, dy);
    sDate.Month   = mo;
    sDate.Date    = dy;
    sDate.Year    = yr;

    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
    HAL_StatusTypeDef ret = HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    if (ret == HAL_OK)
        ret = HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);

    return (ret == HAL_OK) ? 1 : 0;
}

/* ========================================================================== */
/* API 实现                                                                      */
/* ========================================================================== */

void PC_UART_Send(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

void PC_UART_Init(void)
{
    g_UART_RxIdx = 0;
}

void PC_UART_Query(void)
{
    PC_UART_Send("ASK\n");
}

void PC_UART_RequestTime(void)
{
    PC_UART_Send("TIME\n");
}

void PC_UART_ProcessRx(void)
{
    uint8_t ch;
    while (HAL_UART_Receive(&huart1, &ch, 1, 1) == HAL_OK) {
        if (ch == '\n' || ch == '\r') {
            if (g_UART_RxIdx > 0) {
                g_UART_RxBuf[g_UART_RxIdx] = '\0';
                if (g_UART_RxBuf[0] == 'T' && g_UART_RxBuf[1] == 'I') {
                    if (RTC_SetFromSerial(g_UART_RxBuf))
                        g_RTC_Synced = 1;
                } else {
                    const char *p = g_UART_RxBuf;
                    g_PCInfo.cpu_temp  = parse_i8(&p);
                    g_PCInfo.gpu_temp  = parse_i8(&p);
                    g_PCInfo.mem_usage = parse_i8(&p);
                    while (*p && *p != 'P') p++;
                    if (*p) {
                        int8_t h = 0, m = 0;
                        while (*p && (*p < '0' || *p > '9')) p++;
                        while (*p >= '0' && *p <= '9') { h = h * 10 + (*p - '0'); p++; }
                        while (*p && (*p < '0' || *p > '9')) p++;
                        while (*p >= '0' && *p <= '9') { m = m * 10 + (*p - '0'); p++; }
                        fmt_2d(g_PCInfo.uptime,     h); g_PCInfo.uptime[2] = 'h';
                        fmt_2d(g_PCInfo.uptime + 3, m); g_PCInfo.uptime[5] = 'm';
                        g_PCInfo.uptime[6] = '\0';
                    }
                    g_PCInfo.valid = 1;
                }
                g_UART_RxIdx = 0;
            }
        } else if (g_UART_RxIdx < sizeof(g_UART_RxBuf) - 1) {
            g_UART_RxBuf[g_UART_RxIdx++] = (char)ch;
        }
    }
}
