/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* ---- 传感器选择 (二选一) ------------------------------------------------ */
#define USE_DHT11    /* 使用 DHT11 (PA1 单总线), 注释掉此行则使用 AHT20 (I2C) */
/* ------------------------------------------------------------------------ */

#include "lcd_st7032s.h"

#ifdef USE_DHT11
#include "dht11.h"
#else
#include "aht20.h"
#endif

#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* 自定义字符: 笑脸 */

static const uint8_t CGRAM_SMILE[8] = {

    0b00000, 0b01010, 0b01010, 0b00000,

    0b10001, 0b10001, 0b01110, 0b00000,

};

/* 自定义字符: 心形 */

static const uint8_t CGRAM_HEART[8] = {

    0b00000, 0b01010, 0b11111, 0b11111,

    0b11111, 0b01110, 0b00100, 0b00000,

};


/* 自定义字符: 度 ° */

static const uint8_t CGRAM_DEGREE[8] = {

    0b00110, 0b01001, 0b01001, 0b00110,

    0b00000, 0b00000, 0b00000, 0b00000,

};


/* 自定义字符: 反斜杠 \ (替换日系 CGROM 0x5C=¥) */

static const uint8_t CGRAM_BSLASH[8] = {
    0b00001, 0b00010, 0b00100, 0b01000,
    0b10000, 0b00000, 0b00000, 0b00000,
};


/* 自定义字符: 竖线 | (日系 CGROM 0x7C 可能缺失) */

static const uint8_t CGRAM_PIPE[8] = {
    0b00100, 0b00100, 0b00100, 0b00100,
    0b00100, 0b00100, 0b00100, 0b00000,
};


/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* 五项按键状态 (ADC 电压从低到高: Select→Left→Down→Up→Right) */
typedef enum {
    KEY_NONE = 0,
    KEY_SELECT,     /* MRatio 0    → 0V    */
    KEY_LEFT,       /* MRatio 0.20 → 0.67V */
    KEY_DOWN,       /* MRatio 0.40 → 1.32V */
    KEY_UP,         /* MRatio 0.61 → 2.01V */
    KEY_RIGHT       /* MRatio 0.80 → 2.65V */
} KeyState_t;

static volatile KeyState_t g_KeyState = KEY_NONE;

/* PB6 LED 状态 */
static volatile uint8_t  g_LED_State = 0;

/* 显示页面: 0=时间+温湿度, 1=板卡+状态, 2=PC硬件信息 */
#define PAGE_MAX    2
static volatile uint8_t  g_Page = 0;

/* PC 硬件信息 (串口接收) */
typedef struct {
    int8_t  cpu_temp;     /* -1 = N/A */
    int8_t  gpu_temp;     /* -1 = N/A */
    int8_t  mem_usage;    /* -1 = N/A */
    char    uptime[12];
    uint8_t valid;
} PCInfo_t;
static PCInfo_t g_PCInfo = {0};
static uint8_t  g_RTC_Synced = 0;       /* 0=未校准, 1=已校准 */
static uint32_t g_LastPcQuery = 99;     /* 上次查询 PC 的秒数 */

/* UART1 接收缓冲区 */
static char    g_UART_RxBuf[64];
static uint8_t g_UART_RxIdx = 0;

/* 温湿度传感器 */
#ifdef USE_DHT11
static DHT11_Data_t g_Sensor_Data = {0};
#else
static AHT20_Data_t g_Sensor_Data = {0};
#endif
static uint32_t     g_Sensor_LastRead = 0;  /* 上次读取的秒数 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_RTC_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* 五项按键 ADC 读取与判定 */
KeyState_t ADC_ReadKey(void);

/* LED 控制 */
void     LED_On(void);
void     LED_Off(void);
void     LED_Toggle(void);

/* UART1 接收 PC 数据 */
void     UART1_ProcessRx(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#include <string.h>

/* ---- 轻量格式化辅助 ------------------------------------------------------ */
static void fmt_2d(char *p, int8_t v) {
    if (v < 0) v = 0;
    p[0] = '0' + (v / 10);
    p[1] = '0' + (v % 10);
}
static void fmt_1d(char *p, int8_t v) {
    if      (v < 0) v = -v;
    else if (v > 9) v = 9;
    p[0] = '0' + (char)v;
}
static int8_t parse_i8(const char **s) {
    /* 跳过非数字, 解析有符号整数 */
    int8_t v = 0, neg = 0;
    while (**s && (**s < '0' || **s > '9') && **s != '-') (*s)++;
    if (**s == '-') { neg = 1; (*s)++; }
    while (**s >= '0' && **s <= '9') { v = v * 10 + (**s - '0'); (*s)++; }
    return neg ? -v : v;
}

/* ---- UART1 发送字符串 ---------------------------------------------------- */
static void UART1_Send(const char *s) {
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

/* ---- LED 控制 (PB6) ------------------------------------------------------ */
void LED_On(void)  { g_LED_State = 1; HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); }
void LED_Off(void) { g_LED_State = 0; HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); }
void LED_Toggle(void) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_6);
    g_LED_State = (g_LED_State) ? 0 : 1;
}

/* ---- 五项按键 ADC 读取 (PA8, ADC_CHANNEL_8) ------------------------------ */
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

/* ---- 五项按键名称 -------------------------------------------------------- */
static const char* KeyName(KeyState_t key)
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

/* ---- RTC 通过串口校准 -------------------------------------------------- */
static uint8_t RTC_CalcWeekDay(uint8_t y, uint8_t m, uint8_t d)
{
    /* Sakamoto 算法: 返回 0=Sun, 1=Mon, ..., 6=Sat */
    static const int8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t year = 2000 + y;
    if (m < 3) year--;
    uint8_t w = (uint8_t)((year + year/4 - year/100 + year/400 + t[m-1] + d) % 7);
    /* RTC: 1=Mon..7=Sun, 映射: 0→7, 1→1, 2→2, ..., 6→6 */
    return (w == 0) ? RTC_WEEKDAY_SUNDAY : w;
}

static uint8_t RTC_SetFromSerial(const char *s)
{
    /* 格式: "TIME:YYMMDDHHMMSS" (共 17 字符, 不含 \n) */
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

    /* 显式解锁写保护, 确保可写 */
    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
    HAL_StatusTypeDef ret = HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    if (ret == HAL_OK)
        ret = HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);

    return (ret == HAL_OK) ? 1 : 0;
}

/* ---- UART1 接收 PC 数据 (手动解析) --------------------------------------- */
void UART1_ProcessRx(void)
{
    uint8_t ch;
    while (HAL_UART_Receive(&huart1, &ch, 1, 1) == HAL_OK) {
        if (ch == '\n' || ch == '\r') {
            if (g_UART_RxIdx > 0) {
                g_UART_RxBuf[g_UART_RxIdx] = '\0';
                /* TIME 校准命令 */
                if (g_UART_RxBuf[0] == 'T' && g_UART_RxBuf[1] == 'I') {
                    if (RTC_SetFromSerial(g_UART_RxBuf)) {
                        g_RTC_Synced = 1;
                    }
                } else {
                    /* 格式: "CPU:XXC GPU:XXC MEM:XX% UP:XXhXXm" */
                    const char *p = g_UART_RxBuf;
                    g_PCInfo.cpu_temp  = parse_i8(&p);
                    g_PCInfo.gpu_temp  = parse_i8(&p);
                    g_PCInfo.mem_usage = parse_i8(&p);
                    /* 解析 uptime: "XXhXXm" */
                    while (*p && *p != 'P') p++;  /* 跳到 "UP:" */
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

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	char        time_str[17];
    char        lcd_line0[17];
    char        lcd_line1[18];
    KeyState_t  key, key_prev = KEY_NONE;
    uint32_t    last_sec = 99;  /* != RTC初始秒, 触发立即刷新 */
    uint8_t     page_update = 1;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_RTC_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  LCD_Init();
  LCD_CreateChar(0, CGRAM_SMILE);
  LCD_CreateChar(1, CGRAM_HEART);
  LCD_CreateChar(2, CGRAM_DEGREE);
  LCD_CreateChar(3, CGRAM_BSLASH);   /* CGRAM[3] = \ (替代 0x5C ¥) */
  LCD_CreateChar(4, CGRAM_PIPE);     /* CGRAM[4] = | (替代 0x7C) */

  /* 启动 LED 闪烁表示系统就绪 */
  LED_On();
  HAL_Delay(100);
  LED_Off();

  /* UART1 启动信息 */
  UART1_Send("\r\nSTM32C011-DK Ready\r\n");

  /* 设置 RTC 初始时间 (PC 校时前的默认值) */
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
  UART1_Send("TIME\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

      /* ---- 读取 RTC 时间 ---- */
      RTC_TimeTypeDef sTime;
      RTC_DateTypeDef sDate;
      HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
      HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

      /* ---- 每秒更新 ---- */
      if (sTime.Seconds != last_sec) {
          last_sec = sTime.Seconds;
          LED_Toggle();

          /* 拼装时间: " c HH:MM:SS AM c" (手动, 省 flash) */
          {
              static const char FC[4] = {'\004', '/', '-', '\003'};  /* CGRAM[4]=|, CGRAM[3]=\ */
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
          UART1_Send("ASK\n");
      }

      /* ---- UART RX ---- */
      UART1_ProcessRx();

      /* ---- LCD 刷新 ---- */
      if (page_update) {
          page_update = 0;

          switch (g_Page) {
          case 0:
              LCD_Print(0, 0, time_str);
              if (g_Sensor_Data.valid) {
#ifdef USE_DHT11
                  /* DHT11: 仅整数温度/湿度 */
                  int16_t t = (int16_t)g_Sensor_Data.temperature;
                  int16_t h = (int16_t)g_Sensor_Data.humidity;
                  /* "T:XXC H:XX% " */
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
                  /* AHT20: 带一位小数温湿度 */
                  int16_t t = (int16_t)g_Sensor_Data.temperature;
                  int16_t td = (int16_t)(g_Sensor_Data.temperature * 10.0f) % 10;
                  if (td < 0) td = -td;
                  int16_t h = (int16_t)g_Sensor_Data.humidity;
                  int16_t hd = (int16_t)(g_Sensor_Data.humidity * 10.0f) % 10;
                  if (hd < 0) hd = -hd;
                  /* "T:XX.XC H:XX.X%" */
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
                  /* "CPU:XXC GPU:XXC" */
                  memcpy(lcd_line0, "CPU:", 4);
                  fmt_2d(lcd_line0 + 4, g_PCInfo.cpu_temp);
                  lcd_line0[6] = 'C'; lcd_line0[7] = ' ';
                  memcpy(lcd_line0 + 8, "GPU:", 4);
                  fmt_2d(lcd_line0 + 12, g_PCInfo.gpu_temp);
                  lcd_line0[14] = 'C'; lcd_line0[15] = '\0';
                  /* "MEM:XX% UP:XXhXXm" */
                  memcpy(lcd_line1, "MEM:", 4);
                  fmt_2d(lcd_line1 + 4, g_PCInfo.mem_usage);
                  lcd_line1[6] = '%'; lcd_line1[7] = ' ';
                  memcpy(lcd_line1 + 8, "UP:", 3);
                  memcpy(lcd_line1 + 11, g_PCInfo.uptime, 6);
                  lcd_line1[17] = '\0'; /* may overrun, lcd_line1 is 17 */
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
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_1CYCLE_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10805D88;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA4 PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
