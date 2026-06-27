@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set PY="%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not exist %PY% set PY="%LOCALAPPDATA%\Programs\Python\Python313\python.exe"
if not exist %PY% (for /f "delims=" %%i in ('where python 2^>nul') do set PY="%%i")
if not exist %PY% (echo Python not found & pause & exit /b 1)

:: 自动找 STLink COM 口
for /f "tokens=1" %%i in ('%PY% -c "import serial.tools.list_ports;[print(p.device) for p in serial.tools.list_ports.comports() if 'STL' in p.description or 'STLink' in p.description]" 2^>nul') do set "PORT=%%i"
if "%PORT%"=="" (for /f "tokens=1" %%i in ('%PY% -c "import serial.tools.list_ports;print(serial.tools.list_ports.comports()[0].device)" 2^>nul') do set "PORT=%%i")
if "%PORT%"=="" (echo No COM port found & pause & exit /b 1)

echo PC Monitor -> %PORT% @ 115200
echo.

%PY% "%~dp0pc_monitor.py" %PORT%
pause
