/*
 * gnss_types.h —— GNSS 数据类型定义
 *
 * 本头文件定义 GNSS 子系统中使用的基础数据结构：
 *   - GnssFix     : 一次定位结果（位置/速度/精度/卫星数）
 *   - FixType     : 定位解类型枚举（单点/差分的/浮点RTK/固定RTK）
 *   - NeoM8pConfig: NEO-M8P 接收机配置参数
 */

#ifndef GNSS_TYPES_H
#define GNSS_TYPES_H

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace gnss {

/* ── 定位解类型枚举 ────────────────────────────────── */
// 对应 NMEA GGA 语句中的 fix quality 字段
// 值 1=单点, 2=差分, 4=固定RTK, 5=浮点RTK, 其他=无效
enum class FixType {
    Invalid = 0,
    Single = 1,
    Differential = 2,
    FloatRtk = 5,
    FixedRtk = 4
};

/* ── 定位结果结构体 ────────────────────────────────── */
struct GnssFix {
    // 基础状态
    bool valid = false;                    // 定位是否有效
    FixType fixType = FixType::Invalid;    // 定位解类型
    QDateTime timestampUtc;                // UTC 时间戳

    // 位置信息（WGS-84）
    double latitudeDeg = 0.0;              // 纬度（度，北正南负）
    double longitudeDeg = 0.0;             // 经度（度，东正西负）
    double altitudeM = 0.0;                // 海拔高度（米）

    // 运动信息
    double speedMps = 0.0;                // 地面速率（米/秒，来自 RMC）
    double courseDeg = 0.0;               // 地面航向（度，真北，来自 RMC）

    // 精度信息
    int satellites = 0;                    // 参与解算的卫星数
    double hdop = 0.0;                    // 水平精度因子

    // 原始报文（调试用）
    QString sourceSentence;                // 最后一次解析成功的 NMEA 原始语句
};

/* ── 接收机配置 ────────────────────────────────────── */

/**
 * @brief NEO-M8P / 载荷连接配置
 *
 * device   = 设备型号（如 "SRP-Jeston"），传给 ZD_ServerCreate
 * ip       = 载荷 IP（如 "Localhost"），传给 ZD_ServerCreate
 * channelId = 数据通道编号（0-7）
 * updateRateHz = GNSS 数据轮询频率（Hz），控制 QTimer 间隔 = 1000/updateRateHz
 *
 * 串口参数（COM_PORT / BAUD_RATE）由 ZD_ServerCreate 内部从 config.txt 读取，
 * 不在本结构中维护。
 */
struct NeoM8pConfig {
    QString  device  = QStringLiteral("SRP-Jeston");
    QString  ip      = QStringLiteral("Localhost");
    int      channelId = 0;
    int      updateRateHz = 5;
};

} // namespace gnss

// 使 GnssFix 能在跨线程信号/槽中传递
Q_DECLARE_METATYPE(gnss::GnssFix)

#endif // GNSS_TYPES_H
