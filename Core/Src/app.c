/**
 * @file    app.c
 * @brief   Application layer — LCD display, sensors, keys, LED, main loop
 */

#include "app.h"
#include "main.h"
#include "lcd_st7032s.h"
#include "pc_uart.h"
#include <string.h>

/* ========================================================================== */
/* 外部 HAL 句柄                                                                */
/* ========================================================================== */
extern ADC_HandleTypeDef hadc1;
extern RTC_HandleTypeDef  hrtc;
#ifndef USE_DHT11
extern I2C_HandleTypeDef hi2c1;
#endif

/* ========================================================================== */
/* CGRAM 自定义字符                                                              */
/* ========================================================================== */
const uint8_t CGRAM_SMILE[8] = {
    0b00000, 0b01010, 0b01010, 0b00000,
    0b10001, 0b10001, 0b01110, 0b00000,
};

const uint8_t CGRAM_HEART[8] = {
    0b00000, 0b01010, 0b11111, 0b11111,
    0b11111, 0b01110, 0b00100, 0b00000,
};

const uint8_t CGRAM_DEGREE[8] = {
    0b00110, 0b01001, 0b01001, 0b00110,
    0b00000, 0b00000, 0b00000, 0b00000,
};

const uint8_t CGRAM_BSLASH[8] = {
    0b00001, 0b00010, 0b00100, 0b01000,
    0b10000, 0b00000, 0b00000, 0b00000,
};

const uint8_t CGRAM_PIPE[8] = {
    0b00100, 0b00100, 0b00100, 0b00100,
    0b00100, 0b00100, 0b00100, 0b00000,
};

/* ========================================================================== */
/* 模块内部状态                                                                  */
/* ========================================================================== */
static volatile KeyState_t g_KeyState = KEY_NONE;
static volatile uint8_t    g_LED_State = 0;
static volatile uint8_t    g_Page = 0;

#ifdef USE_DHT11
static DHT11_Data_t g_Sensor_Data = {0};
#else
static AHT20_Data_t g_Sensor_Data = {0};
#endif
static uint32_t g_Sensor_LastRead = 0;

/* ========================================================================== */
/* 轻量格式化                                                                    */
/* ========================================================================== */
void fmt_2d(char *p, int8_t v)
{
    if (v < 0) v = 0;
    p[0] = '0' + (v / 10);
    p[1] = '0' + (v % 10);
}

void fmt_1d(char *p, int8_t v)
{
    if      (v < 0) v = -v;
    else if (v > 9) v = 9;
    p[0] = '0' + (char)v;
}

/* ========================================================================== */
/* LED (PB6)                                                                     */
/* ========================================================================== */
void LED_On(void)
{
    g_LED_State = 1;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
}

void LED_Off(void)
{
    g_LED_State = 0;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
}

void LED_Toggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_6);
    g_LED_State = (g_LED_State) ? 0 : 1;
}

/* ========================================================================== */
/* 五项按键 ADC 读取 (PA8, ADC_CHANNEL_8)                                       */
/* ========================================================================== */
KeyState_t ADC_ReadKey(void)
{
    uint32_t adc_val;
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return KEY_NONE;
    }
    adc_val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    if      (adc_val < 410)  return KEY_SELECT;
    else if (adc_val < 1228) return KEY_LEFT;
    else if (adc_val < 2068) return KEY_DOWN;
    else if (adc_val < 2887) return KEY_UP;
    else if (adc_val < 3685) return KEY_RIGHT;
    else                     return KEY_NONE;
}

const char* KeyName(KeyState_t key)
{
    switch (key) {
        case KEY_SELECT: return " SEL";
        case KEY_LEFT:   return "LEFT";
        case KEY_DOWN:   return "DOWN";
        case KEY_UP:     return "  UP";
        case KEY_RIGHT:  return "RIGT";
        default:         return "NONE";
    }
}

/* ========================================================================== */
/* APP_Init                                                                     */
/* ========================================================================== */
void APP_Init(void)
{
    /* LCD + 自定义字符 */
    LCD_Init();
    LCD_CreateChar(0, CGRAM_SMILE);
    LCD_CreateChar(1, CGRAM_HEART);
    LCD_CreateChar(2, CGRAM_DEGREE);
    LCD_CreateChar(3, CGRAM_BSLASH);
    LCD_CreateChar(4, CGRAM_PIPE);

    /* LED 启动闪烁 */
    LED_On();
    HAL_Delay(100);
    LED_Off();

    /* UART1 启动信息 */
    PC_UART_Send("\r\nSTM32C011-DK Ready\r\n");

    /* 设置 RTC 默认时间 (PC 校时前) */
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    sTime.Hours   = 12;
    sTime.Minutes = 0;
    sTime.Seconds = 0;
    sDate.WeekDay = RTC_WEEKDAY_MONDAY;
    sDate.Month   = RTC_MONTH_JANUARY;
    sDate.Date    = 1;
    sDate.Year    = 26;
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
        Error_Handler();
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
        Error_Handler();

    /* 初始化温湿度传感器 */
#ifdef USE_DHT11
    DHT11_Init();
#else
    AHT20_Init(&hi2c1);
#endif

    /* 请求 PC 校准 RTC */
    PC_UART_RequestTime();
}

/* ========================================================================== */
/* APP_Loop                                                                     */
/* ========================================================================== */
void APP_Loop(void)
{
    char        time_str[17];
    char        lcd_line0[17];
    char        lcd_line1[18];
    KeyState_t  key, key_prev = KEY_NONE;
    uint32_t    last_sec = 99;
    uint8_t     page_update = 1;

    while (1)
    {
        /* ---- 读取 RTC 时间 ---- */
        RTC_TimeTypeDef sTime;
        RTC_DateTypeDef sDate;
        HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

        /* ---- 每秒更新 ---- */
        if (sTime.Seconds != last_sec) {
            last_sec = sTime.Seconds;
            LED_Toggle();

            /* 拼装时间: " c HH:MM:SS AM c" */
            {
                static const char FC[4] = {'\004', '/', '-', '\003'};
                static const char* AP[2] = {"AM", "PM"};
                uint8_t h12 = sTime.Hours % 12;
                uint8_t ap  = (sTime.Hours < 12) ? 0 : 1;
                if (h12 == 0) h12 = 12;
                char c = FC[sTime.Seconds & 0x03];
                time_str[0]  = ' ';
                time_str[1]  = c;
                time_str[2]  = ' ';
                fmt_2d(time_str + 3, h12);
                time_str[5]  = ':';
                fmt_2d(time_str + 6, sTime.Minutes);
                time_str[8]  = ':';
                fmt_2d(time_str + 9, sTime.Seconds);
                time_str[11] = ' ';
                time_str[12] = AP[ap][0];
                time_str[13] = AP[ap][1];
                time_str[14] = ' ';
                time_str[15] = c;
                time_str[16] = '\0';
            }

            page_update = 1;
        }

        /* ---- 每 5 秒读取传感器 ---- */
        if ((uint32_t)(sTime.Seconds - g_Sensor_LastRead) >= 5) {
            g_Sensor_LastRead = sTime.Seconds;
#ifdef USE_DHT11
            if (DHT11_Read(&g_Sensor_Data) == HAL_OK && g_Sensor_Data.valid)
#else
            if (AHT20_Read(&hi2c1, &g_Sensor_Data) == HAL_OK && g_Sensor_Data.valid)
#endif
                page_update = 1;
        }

        /* ---- 按键 ---- */
        key = ADC_ReadKey();
        if (key != key_prev) {
            key_prev = key;
            switch (key) {
            case KEY_UP:
                if (g_Page < PAGE_MAX) g_Page++; else g_Page = 0;
                page_update = 1; break;
            case KEY_DOWN:
                if (g_Page > 0) g_Page--; else g_Page = PAGE_MAX;
                page_update = 1; break;
            case KEY_SELECT:
                LED_Toggle();
                page_update = 1; break;
            default: break;
            }
        }

        /* ---- 每 5 秒查询 PC ---- */
        if ((uint32_t)(sTime.Seconds - g_LastPcQuery) >= 5) {
            g_LastPcQuery = sTime.Seconds;
            PC_UART_Query();
        }

        /* ---- UART RX ---- */
        PC_UART_ProcessRx();

        /* ---- LCD 刷新 ---- */
        if (page_update) {
            page_update = 0;

            switch (g_Page) {
            case 0:
                LCD_Print(0, 0, time_str);
                if (g_Sensor_Data.valid) {
#ifdef USE_DHT11
                    /* DHT11: 仅整数 */
                    int16_t t = (int16_t)g_Sensor_Data.temperature;
                    int16_t h = (int16_t)g_Sensor_Data.humidity;
                    memcpy(lcd_line1, "T:", 2);
                    fmt_2d(lcd_line1 + 2, t); lcd_line1[4] = 'C';
                    lcd_line1[5] = ' ';
                    memcpy(lcd_line1 + 6, "H:", 2);
                    fmt_2d(lcd_line1 + 8, h); lcd_line1[10] = '%';
                    lcd_line1[11] = ' ';
                    lcd_line1[12] = ' ';
                    lcd_line1[13] = ' ';
                    lcd_line1[14] = ' ';
                    lcd_line1[15] = '\0';
#else
                    /* AHT20: 带一位小数 */
                    int16_t t = (int16_t)g_Sensor_Data.temperature;
                    int16_t td = (int16_t)(g_Sensor_Data.temperature * 10.0f) % 10;
                    if (td < 0) td = -td;
                    int16_t h = (int16_t)g_Sensor_Data.humidity;
                    int16_t hd = (int16_t)(g_Sensor_Data.humidity * 10.0f) % 10;
                    if (hd < 0) hd = -hd;
                    memcpy(lcd_line1, "T:", 2);
                    fmt_2d(lcd_line1 + 2, t); lcd_line1[4] = '.';
                    fmt_1d(lcd_line1 + 5, td); lcd_line1[6] = 'C';
                    lcd_line1[7] = ' ';
                    memcpy(lcd_line1 + 8, "H:", 2);
                    fmt_2d(lcd_line1 + 10, h); lcd_line1[12] = '.';
                    fmt_1d(lcd_line1 + 13, hd); lcd_line1[14] = '%';
                    lcd_line1[15] = '\0';
#endif
                } else {
                    memcpy(lcd_line1, "T:--.-C H:--.-%", 15);
                    lcd_line1[15] = '\0';
                }
                LCD_Print(0, 1, lcd_line1);
                break;

            case 1:
                LCD_Print(0, 0, " STM32C011-DK   ");
                memcpy(lcd_line1, "LED:", 4);
                memcpy(lcd_line1 + 4, g_LED_State ? "ON " : "OFF", 3);
                lcd_line1[7] = ' ';
                memcpy(lcd_line1 + 8, "KEY:", 4);
                memcpy(lcd_line1 + 12, KeyName(key), 4);
                lcd_line1[16] = '\0';
                LCD_Print(0, 1, lcd_line1);
                break;

            case 2:
                if (g_PCInfo.valid) {
                    memcpy(lcd_line0, "CPU:", 4);
                    fmt_2d(lcd_line0 + 4, g_PCInfo.cpu_temp);
                    lcd_line0[6] = 'C'; lcd_line0[7] = ' ';
                    memcpy(lcd_line0 + 8, "GPU:", 4);
                    fmt_2d(lcd_line0 + 12, g_PCInfo.gpu_temp);
                    lcd_line0[14] = 'C'; lcd_line0[15] = '\0';
                    memcpy(lcd_line1, "MEM:", 4);
                    fmt_2d(lcd_line1 + 4, g_PCInfo.mem_usage);
                    lcd_line1[6] = '%'; lcd_line1[7] = ' ';
                    memcpy(lcd_line1 + 8, "UP:", 3);
                    memcpy(lcd_line1 + 11, g_PCInfo.uptime, 6);
                    lcd_line1[17] = '\0';
                } else {
                    memcpy(lcd_line0, "PC Monitor       ", 16);
                    memcpy(lcd_line1, "Waiting COM...   ", 16);
                }
                LCD_Print(0, 0, lcd_line0);
                LCD_Print(0, 1, lcd_line1);
                break;
            }
        }

        HAL_Delay(50);
    }
}
