# ============================================================================
# pc_monitor.ps1  —  PC 硬件监控 → STM32C011-DK LCD1602 (UART1)
#
# 通过串口发送: CPU温度 GPU温度 内存占用 开机时长
# 协议: 每 3 秒一行 ASCII 明文, 以换行 \n 结束
# 格式: CPU:XXC GPU:XXC MEM:XX% UP:XXhXXm
#
# 用法:
#   .\pc_monitor.ps1 -Port COM3          # 指定端口
#   .\pc_monitor.ps1                      # 自动检测
#   .\pc_monitor.ps1 -List               # 列出可用串口
#   .\pc_monitor.ps1 -Port COM3 -Baud 9600
# ============================================================================

param(
    [string]$Port    = "13",
    [int]   $Baud    = 115200,
    [int]   $Interval = 3,
    [switch]$List
)

# ---- 列出串口 ---------------------------------------------------------------
if ($List) {
    Write-Host "Available COM ports:"
    [System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { Write-Host "  $_" }
    exit 0
}

# ---- 自动检测串口 -----------------------------------------------------------
if ($Port -eq "") {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 0) {
        Write-Host "ERROR: No COM port found. Use -List to check." -ForegroundColor Red
        exit 1
    }
    $Port = $ports[0]
    Write-Host "Auto-detected: $Port"
}

# ---- 打开串口 ---------------------------------------------------------------
try {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, None, 8, One
    $serial.ReadTimeout  = 500
    $serial.WriteTimeout = 1000
    $serial.Open()
    Write-Host "Connected: $Port @ $Baud baud" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Cannot open $Port — $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# ---- 获取 CPU 温度 (℃) -----------------------------------------------------
function Get-CPUTemp {
    try {
        # MSAcpi_ThermalZoneTemperature 返回 Kelvin * 10
        $tz = Get-CimInstance -Namespace root/wmi -ClassName MSAcpi_ThermalZoneTemperature -ErrorAction Stop
        if ($tz) {
            $kelvin = $tz.CurrentTemperature / 10.0
            return [int]($kelvin - 273.15)
        }
    } catch {}
    return -1
}

# ---- 获取 GPU 温度 (℃) -----------------------------------------------------
function Get-GPUTemp {
    # NVIDIA
    try {
        $nvidia = & nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>$null
        if ($nvidia) { return [int]($nvidia.Trim()) }
    } catch {}
    # AMD
    try {
        $amd = Get-CimInstance -Namespace root/wmi -ClassName WMI_AMDGPU 2>$null
        if ($amd) { return [int]$amd.Temperature }
    } catch {}
    # 通用 WMI
    try {
        $gpu = Get-CimInstance -Namespace root/cimv2 -ClassName Win32_VideoController 2>$null
        if ($gpu) { return -2 }  # 有 GPU 但无法读温度
    } catch {}
    return -1
}

# ---- 获取内存占用 (%) -------------------------------------------------------
function Get-MemUsage {
    try {
        $os = Get-CimInstance -ClassName Win32_OperatingSystem
        $total = $os.TotalVisibleMemorySize
        $free  = $os.FreePhysicalMemory
        if ($total -gt 0) {
            return [int]((($total - $free) / $total) * 100)
        }
    } catch {}
    return -1
}

# ---- 获取开机时长 -----------------------------------------------------------
function Get-Uptime {
    try {
        $os = Get-CimInstance -ClassName Win32_OperatingSystem
        $uptime = (Get-Date) - $os.LastBootUpTime
        return "{0:D2}h{1:D2}m" -f [int]$uptime.TotalHours, $uptime.Minutes
    } catch {}
    return "??h??m"
}

# ---- 主循环 -----------------------------------------------------------------
Write-Host ("Sending PC stats every {0}s... (Ctrl+C to stop)" -f $Interval) -ForegroundColor Cyan

while ($true) {
    $cpu  = Get-CPUTemp
    $gpu  = Get-GPUTemp
    $mem  = Get-MemUsage
    $up   = Get-Uptime

    # 格式化: CPU:XXC GPU:XXC MEM:XX% UP:XXhXXm
    $msg  = "CPU:{0}C GPU:{1}C MEM:{2}% UP:{3}" -f $cpu, $gpu, $mem, $up
    $line = $msg + "`n"

    Write-Host ("[{0:HH:mm:ss}] {1}" -f (Get-Date), $msg)
    $serial.Write($line)

    Start-Sleep -Seconds $Interval
}
