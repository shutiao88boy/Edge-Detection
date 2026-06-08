/*
 * nmea_parser.h —— NMEA 0183 协议解析器
 *
 * 支持的报文类型：
 *   - $GxGGA : GPS 定位信息（时间/位置/高度/卫星数/HDOP）
 *   - $GxRMC : 推荐最小定位信息（时间/速度/航向/日期）
 *   其中 "x" 可以是任意 talker ID（如 GP=GPS, GN=多系统）
 *
 * 解析器维护内部状态（m_lastDate），因为 GGA 不含日期，
 * 需从最近接收的 RMC 报文中获取。
 */

#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

#include "gnss_types.h"

#include <QByteArray>
#include <QDate>
#include <QTime>

namespace gnss {

class NmeaParser
{
public:
    /* ── 公开接口 ────────────────────────────────── */

    /**
     * @brief 解析一条 NMEA 0183 语句
     * @param sentence 以 '$' 开头、包含 '*' 校验和的完整 NMEA 行
     * @param fix      输出：解析后的定位结果
     * @return true=解析成功, false=校验失败或语句不识别
     *
     * 调用流程：校验和检查 → 提取字段列表 → 根据类型分发解析
     *   - GGA → 更新位置/高度/卫星/HDOP/FixType
     *   - RMC → 更新日期/速度/航向/位置，GGA 无日期时以此为基准
     */
    bool parseSentence(const QByteArray &sentence, GnssFix *fix);

private:
    /* ── 协议层：校验和验证 ──────────────────────── */
    // 计算 '$' 到 '*' 之间所有字符的异或值，与 '*' 后的两位十六进制对比
    static bool hasValidChecksum(const QByteArray &sentence);

    /* ── 字段解析辅助函数（静态） ────────────────── */
    // NMEA 经纬度格式：DDDMM.mmmm → 十进制度数
    // degreeDigits: 纬度 2 位、经度 3 位
    static double parseCoordinate(const QString &value, const QString &hemisphere);

    // UTC 时间：HHMMSS.ss → QTime（含毫秒）
    static QTime parseUtcTime(const QString &value);

    // RMC 日期：DDMMYY → QDate（自动加 2000）
    static QDate parseRmcDate(const QString &value);

    // GGA fix quality：0=无效,1=单点,2=差分的,4=固定RTK,5=浮点RTK
    static FixType parseGgaFixType(const QString &value);

    /* ── 解析器状态 ──────────────────────────────── */
    GnssFix m_lastFix;   // 上一次的完整定位结果（增量更新用）
    QDate m_lastDate;     // 最近一次 RMC 报文的日期（供 GGA 使用）
};

} // namespace gnss

#endif // NMEA_PARSER_H
