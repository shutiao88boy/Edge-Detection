#!/usr/bin/env python3
"""
NMEA GNSS 模拟器 - 通过串口发送模拟 GNSS 数据
用于在 PC 端模拟 GNSS 接收机，向 Jetson Orin Nano 发送 NMEA 导航数据

用法:
    python nmea_simulator.py COM6             # PC端发送到 COM6 → Jetson
    python nmea_simulator.py /dev/ttyUSB0    # Linux 串口
    python nmea_simulator.py COM6 --baud 115200 --rate 5

依赖:
    pip install pyserial
"""

import argparse
import math
import random
import sys
import time
from datetime import datetime

try:
    import serial
except ImportError:
    print("请先安装 pyserial: pip install pyserial")
    sys.exit(1)


def nmea_checksum(sentence: str) -> str:
    """计算 NMEA 校验和"""
    cksum = 0
    for ch in sentence[1:]:  # 跳过开头的 '$'
        cksum ^= ord(ch)
    return f"{cksum:02X}"


def ddm_to_ddmm(degrees: float, is_lat: bool = True) -> str:
    """
    将十进制度数转为 NMEA 的 DDMM.MMMM 格式
    例如: 31.230416 -> "3113.8250" (纬度)
         121.473701 -> "12128.4221" (经度)
    """
    sign = 1 if degrees >= 0 else -1
    deg = abs(degrees)
    d = int(deg)
    m = (deg - d) * 60.0

    if is_lat:
        return f"{d:02d}{m:07.4f}"
    else:
        return f"{d:03d}{m:07.4f}"


def generate_gga(lat: float, lon: float, alt: float, sats: int, hdop: float) -> str:
    """生成 $GPGGA 语句"""
    now = datetime.utcnow()
    time_str = now.strftime("%H%M%S") + ".00"
    lat_str = ddm_to_ddmm(lat, is_lat=True)
    lon_str = ddm_to_ddmm(lon, is_lat=False)
    lat_ns = "N" if lat >= 0 else "S"
    lon_ew = "E" if lon >= 0 else "W"

    # fix quality: 1=GPS fix, 2=DGPS fix, 4=RTK fixed, 5=RTK float
    fix_quality = 1 if sats < 6 else (2 if sats < 10 else (4 if random.random() > 0.3 else 5))

    fields = [
        "GPGGA",
        time_str,
        lat_str, lat_ns,
        lon_str, lon_ew,
        str(fix_quality),
        f"{sats:02d}",
        f"{hdop:.1f}",
        f"{alt:.1f}", "M",
        "0.0", "M",  # geoidal separation
        "", "",       # DGPS age, station ID
    ]
    sentence = "$" + ",".join(fields)
    return sentence + "*" + nmea_checksum(sentence) + "\r\n"


def generate_rmc(lat: float, lon: float, speed: float, course: float) -> str:
    """生成 $GPRMC 语句"""
    now = datetime.utcnow()
    time_str = now.strftime("%H%M%S") + ".00"
    date_str = now.strftime("%d%m%y")

    lat_str = ddm_to_ddmm(lat, is_lat=True)
    lon_str = ddm_to_ddmm(lon, is_lat=False)
    lat_ns = "N" if lat >= 0 else "S"
    lon_ew = "E" if lon >= 0 else "W"

    # speed in knots
    speed_kts = speed * 1.94384  # m/s -> knots

    fields = [
        "GPRMC",
        time_str,
        "A",  # status: A=valid, V=invalid
        lat_str, lat_ns,
        lon_str, lon_ew,
        f"{speed_kts:.1f}",
        f"{course:.1f}",
        date_str,
        "",  # magnetic variation
        "",  # magnetic direction
    ]
    sentence = "$" + ",".join(fields)
    return sentence + "*" + nmea_checksum(sentence) + "\r\n"


def simulate_movement(t: float):
    """
    模拟无人机运动轨迹
    返回 (lat, lon, alt, speed, course, sats, hdop)
    """
    # 基础位置（上海）
    base_lat = 31.230416
    base_lon = 121.473701
    base_alt = 38.5

    # 圆周运动 + 小幅随机漂移
    radius = 0.0002  # 约 20 米半径
    lat = base_lat + math.sin(t / 8.0) * radius + random.gauss(0, 0.000003)
    lon = base_lon + math.cos(t / 8.0) * radius + random.gauss(0, 0.000003)
    alt = base_alt + math.sin(t / 5.0) * 2.0 + random.gauss(0, 0.3)

    # 速度：约 2-5 m/s
    speed = 3.0 + math.sin(t / 3.0) * 1.5 + abs(random.gauss(0, 0.2))

    # 航向：不断变化
    course = (t * 15 + 90) % 360 + random.gauss(0, 1.0)

    # 卫星数和 HDOP
    sats = random.randint(11, 16)
    hdop = 0.6 + random.random() * 0.4

    return lat, lon, alt, speed, course, sats, hdop


def main():
    parser = argparse.ArgumentParser(description="NMEA GNSS 模拟器 - 通过串口发送模拟导航数据")
    parser.add_argument("port", help="串口名称 (Windows: COM3, Linux: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="波特率 (默认: 115200)")
    parser.add_argument("--rate", type=int, default=5, help="更新频率 Hz (默认: 5)")
    parser.add_argument("--sentences", type=str, default="GGA,RMC",
                        help="发送的 NMEA 语句类型，逗号分隔 (默认: GGA,RMC)")
    args = parser.parse_args()

    sentences_to_send = [s.strip().upper() for s in args.sentences.split(",")]

    print(f"打开串口: {args.port} @ {args.baud} baud")
    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0,
            write_timeout=0
        )
    except Exception as e:
        print(f"无法打开串口: {e}")
        print("\n提示: 如果没有物理串口，可以使用虚拟串口对:")
        print("  Windows: 安装 com0com (https://com0com.sourceforge.net/)")
        print("  Linux:   socat -d -d pty,raw,echo=0 pty,raw,echo=0")
        sys.exit(1)

    print(f"串口已打开，开始发送 NMEA 数据 (更新频率: {args.rate} Hz, 语句: {args.sentences})")
    print("按 Ctrl+C 停止...\n")

    interval = 1.0 / args.rate
    t = 0.0
    count = 0
    start_time = time.time()

    try:
        while True:
            lat, lon, alt, speed, course, sats, hdop = simulate_movement(t)

            lines = []
            if "GGA" in sentences_to_send:
                lines.append(generate_gga(lat, lon, alt, sats, hdop))
            if "RMC" in sentences_to_send:
                lines.append(generate_rmc(lat, lon, speed, course))

            for line in lines:
                ser.write(line.encode("ascii"))

            count += 1
            t += interval

            # 每秒打印一次状态
            if count % args.rate == 0:
                elapsed = time.time() - start_time
                print(f"[{elapsed:6.1f}s] 已发送 {count} 组 | "
                      f"位置: {lat:.6f}, {lon:.6f} | "
                      f"高度: {alt:.1f}m | "
                      f"卫星: {sats} | "
                      f"速度: {speed:.1f}m/s")

            sleep_time = interval - (time.time() % interval) * 0.01
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        print("\n\n已停止。")
    finally:
        ser.close()
        print(f"共发送 {count} 组 NMEA 数据")
        print("串口已关闭。")


if __name__ == "__main__":
    main()
