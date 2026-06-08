/*
 * nmea_parser.cpp —— NMEA 0183 协议解析器实现
 *
 * NMEA 0183 是美国国家海洋电子协会制定的 GPS 接收机数据输出标准。
 * 报文格式：$TALKER,SENTENCE,FIELD1,FIELD2,...*CK\r\n
 *   - $ : 起始符
 *   - TALKER : 设备标识（GP=GPS, GN=GNSS多系统, GL=GLONASS, BD=北斗）
 *   - CK : 两字符十六进制校验和（$ 到 * 之间字符的异或值）
 *
 * 本解析器处理两种核心报文：
 *   GGA — Global Positioning System Fix Data
 *         字段: UTC, 纬度, N/S, 经度, E/W, 质量, 卫星数, HDOP, 高度, ...
 *   RMC — Recommended Minimum Specific GNSS Data
 *         字段: UTC, 状态, 纬度, N/S, 经度, E/W, 速度(节), 航向, 日期, ...
 */

#include "nmea_parser.h"

#include <QStringList>
#include <QtGlobal>

namespace gnss {

/* ═══════════════════════════════════════════════════════
 * 入口：解析一条 NMEA 语句
 * ═══════════════════════════════════════════════════════ */

bool NmeaParser::parseSentence(const QByteArray &sentence, GnssFix *fix)
{
    // 步骤 1：校验和检查（$ 与 * 必须存在且校验和匹配）
    if (!fix || !hasValidChecksum(sentence)) {
        return false;
    }

    // 步骤 2：提取 $ 和 * 之间的有效载荷
    QByteArray payload = sentence.trimmed();
    const int checksumIndex = payload.indexOf('*');
    payload = payload.mid(1, checksumIndex - 1);  // 去掉 '$' 和校验和

    // 步骤 3：逗号分隔字段
    const QStringList fields = QString::fromLatin1(payload).split(',');
    if (fields.isEmpty()) {
        return false;
    }

    // 步骤 4：根据报文类型分发
    const QString type = fields.first();

    // ── GGA 报文解析 ──────────────────────────────
    // 字段索引: 0=类型, 1=UTC, 2=纬度, 3=N/S, 4=经度, 5=E/W,
    //           6=质量, 7=卫星数, 8=HDOP, 9=高度
    if (type.endsWith("GGA") && fields.size() >= 10) {
        // 时间（日期从最近的 RMC 获取）
        m_lastFix.timestampUtc = QDateTime(m_lastDate, parseUtcTime(fields.at(1)), Qt::UTC);
        // 经纬度（NMEA 度分格式 → 十进制度数）
        m_lastFix.latitudeDeg = parseCoordinate(fields.at(2), fields.at(3));
        m_lastFix.longitudeDeg = parseCoordinate(fields.at(4), fields.at(5));
        // 定位质量、卫星数、精度
        m_lastFix.fixType = parseGgaFixType(fields.at(6));
        m_lastFix.valid = m_lastFix.fixType != FixType::Invalid;
        m_lastFix.satellites = fields.at(7).toInt();
        m_lastFix.hdop = fields.at(8).toDouble();
        m_lastFix.altitudeM = fields.at(9).toDouble();
        m_lastFix.sourceSentence = QString::fromLatin1(sentence.trimmed());
        *fix = m_lastFix;
        return true;
    }

    // ── RMC 报文解析 ──────────────────────────────
    // 字段索引: 0=类型, 1=UTC, 2=状态(A=有效/V=无效), 3=纬度, 4=N/S,
    //           5=经度, 6=E/W, 7=速度(节), 8=航向(度), 9=日期
    if (type.endsWith("RMC") && fields.size() >= 10) {
        // 从 RMC 获取日期（GGA 不含日期，需要从这里同步）
        m_lastDate = parseRmcDate(fields.at(9));
        m_lastFix.timestampUtc = QDateTime(m_lastDate, parseUtcTime(fields.at(1)), Qt::UTC);
        // 有效性标志：A=有效, V=警告（无效）
        m_lastFix.valid = fields.at(2) == "A";
        // 经纬度（可能为空，需检查）
        if (!fields.at(3).isEmpty()) {
            m_lastFix.latitudeDeg = parseCoordinate(fields.at(3), fields.at(4));
        }
        if (!fields.at(5).isEmpty()) {
            m_lastFix.longitudeDeg = parseCoordinate(fields.at(5), fields.at(6));
        }
        // 速度：节(knots) → 米/秒（1 knot = 0.514444 m/s）
        m_lastFix.speedMps = fields.at(7).toDouble() * 0.514444;
        // 航向：真北方向，度
        m_lastFix.courseDeg = fields.at(8).toDouble();
        m_lastFix.sourceSentence = QString::fromLatin1(sentence.trimmed());
        *fix = m_lastFix;
        return true;
    }

    // 不支持的报文类型，跳过
    return false;
}

/* ═══════════════════════════════════════════════════════
 * NMEA 校验和验证
 * ═══════════════════════════════════════════════════════ */

/*
 * NMEA 校验和算法：
 *   对 '$' 和 '*' 之间的所有字符（不含 $ 和 *）做逐字节异或(XOR)，
 *   结果与 '*' 后两位十六进制数对比。
 *
 * 示例：$GPGGA,092750.000,...*6E
 *        字符 XOR: 'G'^'P'^'G'^'G'^'A'^','^... = 0x6E ✓
 */
bool NmeaParser::hasValidChecksum(const QByteArray &sentence)
{
    const QByteArray line = sentence.trimmed();

    // 最小长度检查：$ + 至少1字符 + * + 2 hex = 5 字符
    if (line.size() < 4 || line.at(0) != '$') {
        return false;
    }

    // 查找 '*' 校验和分隔符
    const int checksumIndex = line.indexOf('*');
    if (checksumIndex < 0 || checksumIndex + 2 >= line.size()) {
        return false;
    }

    // 计算 XOR 校验和
    quint8 checksum = 0;
    for (int i = 1; i < checksumIndex; ++i) {
        checksum ^= static_cast<quint8>(line.at(i));
    }

    // 与报文中的校验和对比
    bool ok = false;
    const int expected = line.mid(checksumIndex + 1, 2).toInt(&ok, 16);
    return ok && checksum == expected;
}

/* ═══════════════════════════════════════════════════════
 * 字段解析辅助函数
 * ═══════════════════════════════════════════════════════ */

/*
 * 将 NMEA 经纬度格式转换为十进制度数
 *
 * NMEA 格式：DDDMM.mmmm（经度） 或  DDMM.mmmm（纬度）
 *   - 纬度: ddmm.mmmm → dd + mm.mmmm/60
 *   - 经度: dddmm.mmmm → ddd + mm.mmmm/60
 *
 * 半球：N/E 为正，S/W 为负
 */
double NmeaParser::parseCoordinate(const QString &value, const QString &hemisphere)
{
    if (value.isEmpty()) {
        return 0.0;
    }

    // 度数的位数：纬度 2 位（00-90），经度 3 位（000-180）
    const int degreeDigits = (hemisphere == "N" || hemisphere == "S") ? 2 : 3;
    const double degrees = value.left(degreeDigits).toDouble();
    const double minutes = value.mid(degreeDigits).toDouble();

    double coordinate = degrees + minutes / 60.0;

    // 南半球、西半球为负
    if (hemisphere == "S" || hemisphere == "W") {
        coordinate = -coordinate;
    }

    return coordinate;
}

/*
 * 解析 NMEA UTC 时间字符串
 * 格式：HHMMSS.ss → QTime(H, M, S, ms)
 * 支持可选的小数秒部分
 */
QTime NmeaParser::parseUtcTime(const QString &value)
{
    if (value.size() < 6) {
        return {};
    }

    const int hour = value.mid(0, 2).toInt();
    const int minute = value.mid(2, 2).toInt();
    const double secondsValue = value.mid(4).toDouble();
    const int second = static_cast<int>(secondsValue);
    const int msec = qRound((secondsValue - second) * 1000.0);

    return QTime(hour, minute, second, msec);
}

/*
 * 解析 RMC 报文中的日期字段
 * 格式：DDMMYY → QDate（20YY-MM-DD）
 * RMC 不含世纪信息，默认 2000+ 年份
 */
QDate NmeaParser::parseRmcDate(const QString &value)
{
    if (value.size() != 6) {
        return {};
    }

    const int day = value.mid(0, 2).toInt();
    const int month = value.mid(2, 2).toInt();
    const int year = 2000 + value.mid(4, 2).toInt();

    return QDate(year, month, day);
}

/*
 * 将 GGA fix quality 数值映射为 FixType 枚举
 *
 * 映射关系（NMEA 标准）：
 *   0 = 无效
 *   1 = 单点定位（GPS SPS）
 *   2 = 差分定位（DGPS）
 *   4 = 固定解 RTK
 *   5 = 浮点解 RTK
 */
FixType NmeaParser::parseGgaFixType(const QString &value)
{
    switch (value.toInt()) {
    case 1:  return FixType::Single;
    case 2:  return FixType::Differential;
    case 4:  return FixType::FixedRtk;
    case 5:  return FixType::FloatRtk;
    default: return FixType::Invalid;
    }
}

} // namespace gnss
