@echo off
:: ============================================================================
:: pc_monitor.bat  —  双击启动 PC 硬件监控, 后台发送到 STM32C011-DK
::
:: 用法:  双击运行 (自动检测 COM 口)
::        pc_monitor.bat COM3    (指定端口)
::        pc_monitor.bat COM3 9600  (指定端口和波特率)
:: ============================================================================
setlocal

set "PORT=%~1"
set "BAUD=%~2"
if "%PORT%"=="" set "PORT="
if "%BAUD%"=="" set "BAUD=115200"

cd /d "%~dp0"

if "%PORT%"=="" (
    powershell -WindowStyle Minimized -ExecutionPolicy Bypass -File "pc_monitor.ps1" -Baud %BAUD%
) else (
    powershell -WindowStyle Minimized -ExecutionPolicy Bypass -File "pc_monitor.ps1" -Port %PORT% -Baud %BAUD%
)
