/*
 * neo_m8p_receiver.h —— NEO-M8P GNSS 接收机封装
 *
 * 该类封装了与 u-blox NEO-M8P RTK 接收机的交互流程：
 *
 * 数据流：
 *   硬件串口 → C API (ZD_GetGnssData) → 接收环形缓冲
 *     → NMEA 语句分帧 → NmeaParser 解析 → Qt 信号发射
 *
 * 信号：
 *   - fixUpdated(GnssFix)      : 每次成功解析的定位结果
 *   - rawSentenceReceived(QString) : 原始 NMEA 语句（调试用）
 *   - errorOccurred(QString)       : 错误信息
 */

#ifndef NEO_M8P_RECEIVER_H
#define NEO_M8P_RECEIVER_H

#include "gnss_types.h"
#include "nmea_parser.h"

#include <QByteArray>
#include <QObject>
#include <QTimer>

namespace gnss {

class NeoM8pReceiver : public QObject
{
    Q_OBJECT

public:
    explicit NeoM8pReceiver(QObject *parent = nullptr);

    /* ── 生命周期管理 ────────────────────────────── */

    /** @brief 打开接收机连接（串口 + 启动定时轮询） */
    bool open(const NeoM8pConfig &config);

    /** @brief 关闭接收机连接（停止轮询 + 关闭串口） */
    void close();

    /** @return 接收机是否正在运行 */
    bool isOpen() const;

    /** @return 当前配置的副本 */
    NeoM8pConfig config() const;

signals:
    /* ── 数据信号 ────────────────────────────────── */

    /** @brief 成功解析出一次定位结果 */
    void fixUpdated(const GnssFix &fix);

    /** @brief 原始 NMEA 语句（解析前，用于调试） */
    void rawSentenceReceived(const QString &sentence);

    /** @brief 错误信息 */
    void errorOccurred(const QString &message);

private slots:
    /* ── 定时器槽函数 ────────────────────────────── */

    // 由 m_pollTimer 定时触发，轮询 DLL API 获取最新 GNSS 数据
    void pollGnssData();

private:
    /* ── NMEA 分帧处理 ───────────────────────────── */

    // 将缓冲中的一条完整 NMEA 行送入解析器
    void consumeNmeaLine(const QByteArray &line);

    /* ── 成员变量 ────────────────────────────────── */

    QByteArray m_rxBuffer;           // 环形接收缓冲（跨轮询周期累积）
    NmeaParser m_parser;             // NMEA 解析器（含状态）
    NeoM8pConfig m_config;           // 当前配置
    QTimer m_pollTimer;              // 数据轮询定时器
    bool m_running = false;          // 运行状态标记

    /* ── 常量 ────────────────────────────────────── */

    static constexpr int kGnssDataBufferSize = 4096;  // 单次读取最大字节数
    static constexpr int kInfoBufferSize = 1024;      // dataInfo 元信息缓冲区大小
};

} // namespace gnss

#endif // NEO_M8P_RECEIVER_H
