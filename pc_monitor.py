#!/usr/bin/env python3
# ============================================================================
# pc_monitor.py  —  PC 硬件监控 → STM32C011-DK LCD1602 (UART1)
#
# 用法:
#   python pc_monitor.py                  # 自动检测 COM
#   python pc_monitor.py COM3             # 指定端口
#   python pc_monitor.py COM3 9600        # 指定端口+波特率
#   python pc_monitor.py --list           # 列出可用串口
#
# 依赖: pyserial  (pip install pyserial)
#       wmi       (pip install wmi)  — 可选, 增强 GPU 检测
# ============================================================================

import sys
import time
from datetime import datetime
import serial
import serial.tools.list_ports

# ---- 系统信息获取 -----------------------------------------------------------
def cpu_temp():
    """CPU 温度 (℃), 失败返回 -1"""
    try:
        import wmi
        w = wmi.WMI(namespace="root\\wmi")
        temps = w.MSAcpi_ThermalZoneTemperature()
        if temps:
            k = temps[0].CurrentTemperature / 10.0
            return int(k - 273.15)
    except:
        pass
    return -1

def gpu_temp():
    """GPU 温度 (℃), 失败返回 -1"""
    import subprocess
    # NVIDIA
    try:
        r = subprocess.run(["nvidia-smi", "--query-gpu=temperature.gpu",
             "--format=csv,noheader,nounits"],
             capture_output=True, text=True, timeout=5)
        if r.returncode == 0 and r.stdout.strip():
            return int(r.stdout.strip())
    except:
        pass
    # WMI fallback
    try:
        import wmi
        w = wmi.WMI(namespace="root\\cimv2")
        gpus = w.Win32_VideoController()
        if gpus: return -2  # 有 GPU 无温度
    except:
        pass
    return -1

def mem_usage():
    """内存占用 (%)"""
    import ctypes
    try:
        import wmi
        w = wmi.WMI()
        os = w.Win32_OperatingSystem()[0]
        total = int(os.TotalVisibleMemorySize)
        free  = int(os.FreePhysicalMemory)
        return int((total - free) / total * 100)
    except:
        pass
    return -1

def uptime_str():
    """开机时长 "XXhXXm" """
    try:
        import wmi
        w = wmi.WMI()
        os = w.Win32_OperatingSystem()[0]
        boot = os.LastBootUpTime
        # WMI 格式: "20260101120000.000000+480"
        boot_dt = datetime.strptime(boot.split('.')[0], "%Y%m%d%H%M%S")
        delta = datetime.now() - boot_dt
        h = int(delta.total_seconds() // 3600)
        m = int((delta.total_seconds() % 3600) // 60)
        return f"{h:02d}h{m:02d}m"
    except:
        pass
    return "??h??m"

# ---- 主程序 -----------------------------------------------------------------
def main():
    if "--list" in sys.argv:
        ports = serial.tools.list_ports.comports()
        print("Available COM ports:")
        for p in ports:
            print(f"  {p.device} — {p.description}")
        return

    # 端口
    port = "COM25"  # default
    baud = 115200
    interval = 3

    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        port = sys.argv[1]
    if len(sys.argv) > 2 and not sys.argv[2].startswith("-"):
        baud = int(sys.argv[2])

    # 自动检测
    if port == "COM3" or not any(c in port for c in "COM"):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if ports:
            port = ports[0]
            print(f"Auto-detected: {port}")

    try:
        ser = serial.Serial(port, baud, timeout=0.5, write_timeout=1)
        print(f"Connected: {port} @ {baud} baud")
    except Exception as e:
        print(f"ERROR: {e}")
        return

    # ---- RTC 校时 ---------------------------------------------------------
    time.sleep(2)  # 等待 STM32 初始化完成
    try:
        now = datetime.now()
        ts = now.strftime("TIME:%y%m%d%H%M%S")
        ser.write((ts + "\n").encode())
        resp = ser.readline().decode(errors='ignore').strip()
        print(f"RTC sync: {ts}  →  {resp}")
    except Exception as e:
        print(f"RTC sync failed: {e}")

    print(f"Sending PC stats every {interval}s... (Ctrl+C to stop)")
    try:
        while True:
            cpu = cpu_temp()
            gpu = gpu_temp()
            mem = mem_usage()
            up  = uptime_str()
            msg = f"CPU:{cpu}C GPU:{gpu}C MEM:{mem}% UP:{up}\n"
            now = time.strftime("%H:%M:%S")
            print(f"[{now}] {msg.strip()}")
            ser.write(msg.encode())
            time.sleep(interval)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()5
