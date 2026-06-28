# STM32C011-DK LCD1602 Monitor

基于 STM32C011F6 的 1602 LCD 显示终端，集成温湿度传感器、RTC 时钟和 PC 硬件监控功能。

## 硬件配置

| 功能 | 外设 | 引脚 | 说明 |
|------|------|------|------|
| LCD 显示 | SPI1 | PA5/SCK, PA7/MOSI | ST7032S 驱动 (JLX1602G-916 1602 COG) |
| LCD 片选 | GPIO | PA4/CS, PA6/RS | 软件控制 |
| 温湿度 (DHT11) | GPIO | PA1 | DHT11 单总线 (默认) |
| 温湿度 (AHT20) | I2C1 | PB7/SCL, PC14/SDA | AHT20 传感器 (0x38, 备选) |
| 五项按键 | ADC1 | PA8/CH8 | 分压按键 (Select/Left/Down/Up/Right) |
| 状态灯 | GPIO | PB6 | LED 指示 |
| PC 通信 | USART1 | PA2/TX, PA3/RX | 接收 PC 硬件信息 (115200) |
| 调试串口 | USART2 | PA9/TX, PA10/RX | SWD/调试 (115200) |
| RTC | RTC | - | LSI 时钟源 |

## 功能特性

- **页面 0** — 实时时钟 + 温湿度显示

通过 UP/DOWN 按键切换页面。

### 传感器切换

`main.c` 顶部通过宏选择传感器：

```c
#define USE_DHT11    /* 使用 DHT11 (PA1 单总线), 注释掉此行则使用 AHT20 (I2C) */
```

- **DHT11**（默认）— 单总线协议，数据线 PA1，需外部 4.7kΩ~10kΩ 上拉电阻
- **AHT20** — I2C 协议，地址 0x38，使用 I2C1 (PB7/SCL, PC14/SDA)
- **页面 1** — 板卡状态 (LED + 按键状态)
- **页面 2** — PC 硬件监控 (CPU/GPU 温度、内存占用、开机时长)

通过 UP/DOWN 按键切换页面。

## LCD 自定义字符

| CGRAM | 字符 | 说明 |
|-------|------|------|
| 0 | ☺ | 笑脸 |
| 1 | ♥ | 心形 |
| 2 | ° | 摄氏度符号 |
| 3 | \ | 反斜杠 (日系 CGROM 0x5C=¥) |
| 4 | \| | 竖线 |

## PC 监控脚本

项目提供三种语言的 PC 端串口发送脚本，将 CPU/GPU 温度、内存占用、开机时长通过串口发送给 STM32 显示：

```bash
# Python (推荐)
python pc_monitor.py              # 自动检测端口
python pc_monitor.py COM3         # 指定端口
python pc_monitor.py --list       # 列出可用端口

# PowerShell
.\pc_monitor.ps1                  # 自动检测端口

# Batch
启动PC监控.bat                     # 直接启动 (默认COM3)
```

### 数据协议

串口格式 (115200 8N1)，以 `\n` 结尾：

```
CPU:45C GPU:52C MEM:67% UP:02h35m
```

## 开发环境

- **IDE**: STM32CubeIDE / STM32CubeMX
- **MCU**: STM32C011F6Ux (Cortex-M0+, 48MHz)
- **HAL**: STM32Cube FW_C0 V1.1.0
- **编译器**: ARM GCC (arm-none-eabi-gcc)

## 项目结构

```
C011_LCD/
├── Core/
│   ├── Inc/              # 头文件 (main.h, stm32c0xx_hal_conf.h, stm32c0xx_it.h)
│   ├── Src/              # 源文件
│   │   ├── main.c        # 主程序
│   │   ├── lcd_st7032s.c # LCD ST7032S 驱动
│   │   ├── dht11.c       # DHT11 温湿度传感器驱动 (默认)
│   │   ├── aht20.c       # AHT20 温湿度传感器驱动 (备选)
│   │   ├── stm32c0xx_it.c
│   │   ├── stm32c0xx_hal_msp.c
│   │   ├── syscalls.c
│   │   └── sysmem.c
│   └── Startup/          # 启动文件
│       └── startup_stm32c011f6ux.s
├── Drivers/
│   ├── STM32C0xx_HAL_Driver/  # HAL 库
│   └── CMSIS/                 # CMSIS
├── C011_LCD.ioc          # CubeMX 工程文件
├── STM32C011F6UX_FLASH.ld    # 链接脚本
├── pc_monitor.py          # Python 监控脚本
├── pc_monitor.ps1         # PowerShell 监控脚本
├── pc_monitor.bat         # Batch 监控脚本
├── 启动PC监控.bat          # 一键启动批处理
├── 启动PC监控_静默.vbs     # 静默后台启动
└── 1602G-916-BN中文说明书.pdf  # LCD 模组数据手册
```

## 许可证

MIT License
