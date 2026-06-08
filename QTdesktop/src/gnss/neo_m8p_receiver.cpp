/*
 * neo_m8p_receiver.cpp —— NEO-M8P GNSS 接收机封装实现
 *
 * 完整数据链路：
 *
 *   串口硬件                            NeoM8pReceiver (本类)
 *   ──────────                          ────────────────────
 *   NMEA 字节流 → C API 轮询 ─┐
 *                             ├→ m_pollTimer(定时器)
 *                             │     │
 *                             │   pollGnssData()
 *                             │     ├─ ZD_GetGnssData() → 读取原始字节
 *                             │     ├─ 追加到 m_rxBuffer (环形缓冲)
 *                             │     └─ 从缓冲中搜索 '$'~'\n' 的完整 NMEA 行
 *                             │           │
 *                             │      consumeNmeaLine()
 *                             │         ├─ emit rawSentenceReceived()  原始报文
 *                             │         └─ m_parser.parseSentence()
 *                             │               └─ emit fixUpdated()     定位结果
 *                             │
 *   NeoM8pConfig ─────────────┘  open() 时传入，含轮询频率/通道号
 *
 * 缓冲处理说明：
 *   串口为非阻塞读取，单次 read 可能只返回部分数据（例如半条 NMEA 线）。
 *   m_rxBuffer 在多次轮询间累积数据，确保能拼出完整的 $...\n 行再解析。
 */

#include "neo_m8p_receiver.h"

#include <cstring>

// C API 头文件（extern "C" 链接）
extern "C" {
#include "signal_edge_ui_api.h"
}

namespace gnss {

/* ═══════════════════════════════════════════════════════
 * 构造/析构
 * ═══════════════════════════════════════════════════════ */

NeoM8pReceiver::NeoM8pReceiver(QObject *parent)
    : QObject(parent)
{
    // 连接定时器超时信号到轮询槽函数
    connect(&m_pollTimer, &QTimer::timeout, this, &NeoM8pReceiver::pollGnssData);
}

/* ═══════════════════════════════════════════════════════
 * 连接管理
 * ═══════════════════════════════════════════════════════ */

/*
 * open —— 打开 NEO-M8P 接收机连接
 *
 * 流程：
 *   1. 关闭旧连接（如果存在）
 *   2. 保存配置
 *   3. 调用 ZD_ServerCreate 打开串口（内部从配置文件加载参数）
 *   4. 根据配置的 updateRateHz 计算轮询间隔并启动定时器
 *
 * 返回值：true=成功, false=失败（通过 errorOccurred 信号通知错误原因）
 */
bool NeoM8pReceiver::open(const NeoM8pConfig &config)
{
    // 先关闭旧连接，确保状态干净
    close();

    m_config = config;

    // 将 Qt 字符串转换为 C 字符串（UTF-8 编码）
    QByteArray deviceBytes = config.device.toLocal8Bit();
    QByteArray ipBytes = config.ip.toLocal8Bit();

    // 调用 C API 创建连接（串口参数由 ZD_ServerCreate 内部从 config.txt 读取）
    int ret = ZD_ServerCreate(deviceBytes.data(), ipBytes.data());
    if (ret != 0) {
        // 连接失败，通过信号通知上层
        emit errorOccurred(QString::fromLocal8Bit(ZD_GetLastError()));
        return false;
    }

    m_running = true;

    // 计算轮询间隔（毫秒），例如 5Hz → 200ms
    const int intervalMs = config.updateRateHz > 0 ? 1000 / config.updateRateHz : 200;
    m_pollTimer.start(intervalMs);

    return true;
}

/*
 * close —— 关闭连接，释放资源
 */
void NeoM8pReceiver::close()
{
    // 停止定时器（不再触发 pollGnssData）
    m_pollTimer.stop();
    m_running = false;

    // 清空接收缓冲
    m_rxBuffer.clear();

    // 关闭串口连接
    ZD_ServerDestroy();
}

bool NeoM8pReceiver::isOpen() const
{
    return m_running;
}

NeoM8pConfig NeoM8pReceiver::config() const
{
    return m_config;
}

/* ═══════════════════════════════════════════════════════
 * 数据轮询
 * ═══════════════════════════════════════════════════════ */

/*
 * pollGnssData —— 定时器触发的核心轮询函数
 *
 * 每次被 m_pollTimer 触发时执行以下步骤：
 *
 *   1. 调用 ZD_GetGnssData() 从串口读取原始字节
 *   2. 将字节追加到 m_rxBuffer 环形缓冲
 *   3. 在缓冲中查找完整的 NMEA 句子：
 *        - 从 '$' 开始，到 '\n' 结束
 *        - 非 NMEA 数据（$ 之前的垃圾字节）自动丢弃
 *        - 不完整的行保留在缓冲中等下次轮询
 *   4. 对每条完整句子调用 consumeNmeaLine()
 */
void NeoM8pReceiver::pollGnssData()
{
    if (!m_running) {
        return;
    }

    // 步骤 1：通过 C API 从串口读取原始 GNSS 数据
    QByteArray gnssData(kGnssDataBufferSize, Qt::Uninitialized);
    gnssData.fill(0);
    QByteArray dataInfo(kInfoBufferSize, Qt::Uninitialized);
    dataInfo.fill(0);

    int ret = ZD_GetGnssData(m_config.channelId, gnssData.data(),
                             kGnssDataBufferSize, dataInfo.data());
    if (ret != 0) {
        emit errorOccurred(QString::fromLocal8Bit(ZD_GetLastError()));
        return;
    }

    // 计算实际数据长度（以 \0 终止时的有效数据）
    int dataLen = static_cast<int>(qstrnlen(gnssData.constData(), kGnssDataBufferSize));
    if (dataLen == 0) {
        return;  // 无新数据
    }

    // 步骤 2：追加到接收缓冲
    m_rxBuffer.append(gnssData.left(dataLen));

    // 步骤 3：从缓冲中提取完整 NMEA 行
    while (!m_rxBuffer.isEmpty()) {
        // 查找 '$' 起始位置
        const int nmeaStart = m_rxBuffer.indexOf('$');
        if (nmeaStart < 0) {
            // 缓冲中没有任何 NMEA 句子，清空
            m_rxBuffer.clear();
            return;
        }

        // '$' 之前的垃圾数据直接丢弃
        if (nmeaStart > 0) {
            m_rxBuffer.remove(0, nmeaStart);
        }

        // 查找换行符（NMEA 行结束标志）
        const int lineEnd = m_rxBuffer.indexOf('\n');
        if (lineEnd < 0) {
            // 行不完整，等待下次轮询补充数据
            return;
        }

        // 提取完整行
        const QByteArray line = m_rxBuffer.left(lineEnd + 1);
        m_rxBuffer.remove(0, lineEnd + 1);

        // 步骤 4：送入解析器处理
        consumeNmeaLine(line);
    }
}

/* ═══════════════════════════════════════════════════════
 * NMEA 语句消费
 * ═══════════════════════════════════════════════════════ */

/*
 * consumeNmeaLine —— 处理一条完整的 NMEA 语句
 *
 * 1. 发射 rawSentenceReceived 信号（调试用，可查看原始报文）
 * 2. 调用 NmeaParser 解析
 * 3. 解析成功时发射 fixUpdated 信号（UI 层通过此信号更新显示）
 *
 * 注意：校验和错误或非 GGA/RMC 的句子不会被解析，但仍会发射原始信号
 */
void NeoM8pReceiver::consumeNmeaLine(const QByteArray &line)
{
    // 发射原始报文信号（调试/日志用）
    const QString sentence = QString::fromLatin1(line.trimmed());
    emit rawSentenceReceived(sentence);

    // 尝试解析为定位结果
    GnssFix fix;
    if (m_parser.parseSentence(line, &fix)) {
        emit fixUpdated(fix);
    }
}

} // namespace gnss
