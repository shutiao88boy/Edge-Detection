/*
 * mainwindow.cpp —— 主窗口实现
 *
 * ═══════════════ 架构概述 ═══════════════
 *
 *   用户操作          MainWindow                 数据流
 *   ──────────        ─────────────────────       ──────────────────────
 *   按钮点击 ─────→ on_btnXxx_clicked() ─────→ 模块开关 → 定时器启停
 *                                                         │
 *   ┌─ GNSS 数据（真实）──────────────────────────────────┘
 *   │  NeoM8pReceiver::pollGnssData()
 *   │    → 信号 fixUpdated → onGnssFixUpdated() → UI 更新
 *   │
 *   ├─ INS 数据（模拟）───────────────────────────────────
 *   │  m_sensorTimer::timeout → updateSensorData()
 *   │    → 三角函数生成 → UI 更新
 *   │
 *   └─ 图像数据（模拟）───────────────────────────────────
 *      m_imageTimer::timeout → updateImageData()
 *        → QPainter 绘制 → label->setPixmap()
 *
 * ═══════════════ 定时器智能管理 ═══════════════
 *
 *   m_sensorTimer (500ms) : m_gnssRunning || m_insRunning
 *   m_imageTimer  (40ms)  : m_image1Running || m_image2Running
 *
 *   所有模块都停止时，对应定时器自动停止，节省 CPU。
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "gnss_types.h"
#include "neo_m8p_receiver.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <QTextStream>
#include <QtMath>

/* ═══════════════════════════════════════════════════════
 * 构造 / 析构
 * ═══════════════════════════════════════════════════════ */

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 图像标签的尺寸策略：允许拉伸，最低高度 220px
    ui->labelImage1->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    ui->labelImage2->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    ui->labelImage1->setMinimumHeight(220);
    ui->labelImage2->setMinimumHeight(220);

    // 初始化各面板默认文本
    resetGnssText();
    resetInsText();

    // 从配置文件恢复串口设置到 UI 控件
    loadGnssConfig();

    // 创建 GNSS 接收机实例，连接信号到槽
    m_gnssReceiver = new gnss::NeoM8pReceiver(this);
    connect(m_gnssReceiver, &gnss::NeoM8pReceiver::fixUpdated,
            this, &MainWindow::onGnssFixUpdated);
    connect(m_gnssReceiver, &gnss::NeoM8pReceiver::errorOccurred,
            this, &MainWindow::onGnssError);

    // 连接定时器信号（定时器初始为停止状态，模块启动后才触发）
    connect(&m_sensorTimer, &QTimer::timeout, this, &MainWindow::updateSensorData);
    connect(&m_imageTimer, &QTimer::timeout, this, &MainWindow::updateImageData);

    // 设置定时器默认间隔
    m_sensorTimer.setInterval(500);   // 传感器：2Hz
    m_imageTimer.setInterval(40);     // 图像：25fps
}

MainWindow::~MainWindow()
{
    // 优雅关闭 GNSS 连接（停止定时器 + 关闭串口）
    if (m_gnssReceiver) {
        m_gnssReceiver->close();
    }
    delete ui;
}

/* ═══════════════════════════════════════════════════════
 * 模块开关按钮
 * ═══════════════════════════════════════════════════════ */

/*
 * GNSS 模块开关
 *
 * 开启流程：
 *   1. 检查配置文件是否存在有效串口参数
 *   2. 构造 NeoM8pConfig（设备型号 SRP-Jeston，载荷 IP Localhost）
 *   3. 调用 NeoM8pReceiver::open() → 内部调用 ZD_ServerCreate 打开串口
 *   4. 串口参数（COM_PORT/BAUD_RATE）由 ZD_ServerCreate 从 config.ini 加载
 *
 * 关闭流程：
 *   1. 停止定时轮询 + 关闭串口
 *   2. 重置 GNSS 显示为默认无信号文本
 */
void MainWindow::on_btnGnss_clicked()
{
    m_gnssRunning = !m_gnssRunning;

    if (m_gnssRunning) {
        // 构造接收机配置
        // 串口参数（COM_PORT/BAUD_RATE）由 ZD_ServerCreate 内部从 config.txt 读取
        gnss::NeoM8pConfig cfg;
        cfg.device    = "SRP-Jeston";
        cfg.ip        = "Localhost";
        cfg.channelId = 0;
        cfg.updateRateHz = 5;

        if (!m_gnssReceiver->open(cfg)) {
            m_gnssRunning = false;
            return;
        }
    } else {
        // 关闭连接，恢复默认显示
        m_gnssReceiver->close();
        resetGnssText();
    }

    // 更新状态指示
    setButtonState(m_gnssRunning, ui->labelGnssStatus);
    updateTimers();
}

/*
 * INS 模块开关（模拟数据）
 * 无真实硬件连接，直接使用三角函数生成模拟传感器值
 */
void MainWindow::on_btnIns_clicked()
{
    m_insRunning = !m_insRunning;
    setButtonState(m_insRunning, ui->labelInsStatus);
    if (!m_insRunning) {
        resetInsText();
    }
    updateTimers();
}

/*
 * 图像第 1 路开关（模拟视频）
 */
void MainWindow::on_btnImage1_clicked()
{
    m_image1Running = !m_image1Running;
    setButtonState(m_image1Running, ui->labelImage1Status);
    if (!m_image1Running) {
        ui->labelImage1->clear();
        ui->labelImage1->setText("图像第1路显示区域\n(暂无图像)");
    }
    updateTimers();
}

/*
 * 图像第 2 路开关（模拟视频）
 */
void MainWindow::on_btnImage2_clicked()
{
    m_image2Running = !m_image2Running;
    setButtonState(m_image2Running, ui->labelImage2Status);
    if (!m_image2Running) {
        ui->labelImage2->clear();
        ui->labelImage2->setText("图像第2路显示区域\n(暂无图像)");
    }
    updateTimers();
}

/* ═══════════════════════════════════════════════════════
 * 定时器回调
 * ═══════════════════════════════════════════════════════ */

/*
 * updateSensorData —— 传感器定时器回调（每 500ms 触发一次）
 *
 * 注意：GNSS 数据不在此处处理，GNSS 由 NeoM8pReceiver 的内部定时器
 * 独立轮询并通过信号异步驱动 onGnssFixUpdated() 更新 UI。
 *
 * INS 模拟数据：使用三角函数生成类真实传感器输出
 *   - 陀螺仪：±0.2 deg/s 左右随机摆动
 *   - 加速度：约 9.8 m/s² 基础重力 + 小幅随机抖动
 *   - 姿态角：缓慢变化的角度模拟
 */
void MainWindow::updateSensorData()
{
    m_tick += 0.5;  // 每 500ms 增加 0.5 秒

    /*
     * GNSS 数据由 NeoM8pReceiver 的信号驱动更新（onGnssFixUpdated），
     * 不在此处模拟。
     */

    if (m_insRunning) {
        // 生成模拟传感器数据（三角函数相位偏移避免重复）
        const double gx = qSin(m_tick) * 0.18;
        const double gy = qCos(m_tick / 1.7) * 0.15;
        const double gz = qSin(m_tick / 2.3) * 0.12;
        const double ax = qSin(m_tick / 3.0) * 0.03;
        const double ay = qCos(m_tick / 2.0) * 0.03;
        const double az = 9.806 + qSin(m_tick / 4.0) * 0.04;  // 重力 + 振动
        const double roll = qSin(m_tick / 5.0) * 2.5;
        const double pitch = qCos(m_tick / 5.5) * 2.0;
        const double yaw = std::fmod(m_tick * 2.0, 360.0);

        ui->textIns->setPlainText(QString(
            "惯性导航系统\n"
            "陀螺仪：%1, %2, %3 deg/s\n"
            "加速度：%4, %5, %6 m/s²\n"
            "角度：%7, %8, %9 °\n"
            "速度：N/A")
            .arg(gx, 0, 'f', 3)
            .arg(gy, 0, 'f', 3)
            .arg(gz, 0, 'f', 3)
            .arg(ax, 0, 'f', 3)
            .arg(ay, 0, 'f', 3)
            .arg(az, 0, 'f', 3)
            .arg(roll, 0, 'f', 2)
            .arg(pitch, 0, 'f', 2)
            .arg(yaw, 0, 'f', 2));
    }
}

/*
 * updateImageData —— 图像定时器回调（每 40ms 触发一次）
 *
 * 分别绘制两路模拟视频帧，每路显示：
 *   - 深色背景 + 移动网格线（模拟场景结构）
 *   - 正弦运动的小圆球（模拟目标跟踪点）
 *   - 帧号 和 当前时间戳
 */
void MainWindow::updateImageData()
{
    ++m_frameNo;
    if (m_image1Running) {
        drawImage(ui->labelImage1, 1);
    }
    if (m_image2Running) {
        drawImage(ui->labelImage2, 2);
    }
}

/* ═══════════════════════════════════════════════════════
 * GNSS 信号处理
 * ═══════════════════════════════════════════════════════ */

/*
 * onGnssFixUpdated —— 收到有效的 GNSS 定位结果
 *
 * 将 GnssFix 结构体中的字段格式化为可读文本，
 * 更新卫星导航面板显示。
 */
void MainWindow::onGnssFixUpdated(const gnss::GnssFix &fix)
{
    if (!m_gnssRunning || !fix.valid) return;

    // 将定位解类型枚举映射为中文字符串
    const char *fixTypeStr = "未知";
    switch (fix.fixType) {
    case gnss::FixType::Single:       fixTypeStr = "单点定位"; break;
    case gnss::FixType::Differential: fixTypeStr = "差分定位"; break;
    case gnss::FixType::FloatRtk:     fixTypeStr = "浮点RTK";  break;
    case gnss::FixType::FixedRtk:     fixTypeStr = "固定RTK";  break;
    default:                          fixTypeStr = "无效";     break;
    }

    ui->textGnss->setPlainText(QString(
        "卫星导航系统\n"
        "卫星信号：GPS-L1 / BDS-B1\n"
        "卫星数量：%1\n"
        "定位状态：%2\n"
        "经纬度：%3, %4\n"
        "高度：%5 m\n"
        "速度：%6 m/s\n"
        "航向：%7 °\n"
        "HDOP：%8\n"
        "时间：%9 UTC")
        .arg(fix.satellites)
        .arg(fixTypeStr)
        .arg(fix.latitudeDeg, 0, 'f', 7)
        .arg(fix.longitudeDeg, 0, 'f', 7)
        .arg(fix.altitudeM, 0, 'f', 2)
        .arg(fix.speedMps, 0, 'f', 2)
        .arg(fix.courseDeg, 0, 'f', 1)
        .arg(fix.hdop, 0, 'f', 1)
        .arg(fix.timestampUtc.isValid()
             ? fix.timestampUtc.toString("hh:mm:ss")
             : "N/A"));
}

/*
 * onGnssError —— GNSS 模块错误处理
 *
 * 直接显示错误信息到卫星导航面板
 */
void MainWindow::onGnssError(const QString &message)
{
    ui->textGnss->setPlainText(QString("卫星导航系统\n错误：%1").arg(message));
}

/* ═══════════════════════════════════════════════════════
 * 图像绘制
 * ═══════════════════════════════════════════════════════ */

/*
 * drawImage —— 生成一路模拟视频帧并设置到 QLabel
 *
 * 绘制内容（模拟机载摄像头画面）：
 *   1. 深色背景 (#1C1E1F)
 *   2. 滚动的网格线（偏移量随帧号和通道变化）
 *   3. 正弦运动的小圆球（颜色区分的"跟踪目标"）
 *   4. 左上角：通道标识 + 帧号
 *   5. 左下角：当前时间戳
 */
void MainWindow::drawImage(QLabel *label, int channel)
{
    // 获取目标显示区域尺寸
    const QSize targetSize = label->contentsRect().size();
    if (targetSize.isEmpty()) {
        return;
    }

    QPixmap pixmap(targetSize);
    pixmap.fill(QColor(28, 30, 31));  // 深色背景

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const int width = pixmap.width();
    const int height = pixmap.height();

    // 网格线偏移量（随帧号滚动，制造移动效果）
    const int offsetX = (m_frameNo * (channel + 1)) % 40;
    const int offsetY = (m_frameNo * (channel + 2)) % 40;

    // 绘制网格线
    painter.setPen(QPen(QColor(64, 94, 105), 1));
    for (int x = offsetX; x < width; x += 40) {
        painter.drawLine(x, 0, x, height);
    }
    for (int y = offsetY; y < height; y += 40) {
        painter.drawLine(0, y, width, y);
    }

    // 绘制运动小圆球（模拟跟踪目标）
    // 通道 1：橙色，通道 2：绿色
    const int cx = width / 2 + qRound(qSin(m_frameNo / (10.0 + channel)) * width * 0.25);
    const int cy = height / 2 + qRound(qCos(m_frameNo / (13.0 + channel)) * height * 0.20);
    painter.setBrush(channel == 1 ? QColor(255, 152, 0, 180) : QColor(67, 160, 71, 180));
    painter.setPen(QPen(QColor(230, 230, 230), 2));
    painter.drawEllipse(QPoint(cx, cy), 26, 26);

    // 绘制文本叠加层
    painter.setPen(QColor(235, 235, 235));
    painter.drawText(12, 24, QString("IMAGE %1  SIM VIDEO").arg(channel));
    painter.drawText(12, 46, QString("Frame: %1").arg(m_frameNo));
    painter.drawText(12, height - 14, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    // 设置到 QLabel 显示
    label->setPixmap(pixmap);
}

/* ═══════════════════════════════════════════════════════
 * UI 辅助方法
 * ═══════════════════════════════════════════════════════ */

/*
 * setButtonState —— 更新模块状态标签
 * running=true  → 绿色 "状态：开启"
 * running=false → 灰色 "状态：关闭"
 */
void MainWindow::setButtonState(bool running, QLabel *statusLabel)
{
    statusLabel->setText(running ? "状态：开启" : "状态：关闭");
    statusLabel->setStyleSheet(QString("font: 12px \"Microsoft YaHei\"; color: %1;")
                                   .arg(running ? "#1b8d42" : "#666666"));
}

/*
 * 重置 GNSS 面板为默认无信号状态
 */
void MainWindow::resetGnssText()
{
    ui->textGnss->setPlainText(
        "卫星导航系统\n"
        "卫星信号：无\n"
        "定位状态：未定位\n"
        "经纬度：N/A\n"
        "高度：N/A");
}

/*
 * 重置 INS 面板为默认零值状态
 */
void MainWindow::resetInsText()
{
    ui->textIns->setPlainText(
        "惯性导航系统\n"
        "陀螺仪：0, 0, 0\n"
        "加速度：0, 0, 0\n"
        "角度：0, 0, 0\n"
        "速度：N/A");
}

/*
 * updateTimers —— 根据模块运行状态智能启停定时器
 *
 * m_sensorTimer：GNSS 或 INS 任一开启时运行
 * m_imageTimer：图像 1 或图像 2 任一开启时运行
 */
void MainWindow::updateTimers()
{
    if (m_gnssRunning || m_insRunning) {
        m_sensorTimer.start();
    } else {
        m_sensorTimer.stop();
    }

    if (m_image1Running || m_image2Running) {
        m_imageTimer.start();
    } else {
        m_imageTimer.stop();
    }
}

/* ═══════════════════════════════════════════════════════
 * 配置文件管理
 * ═══════════════════════════════════════════════════════ */

/*
 * 获取 config.txt 的完整路径（可执行文件同目录）
 */
QString MainWindow::configFilePath() const
{
    return QCoreApplication::applicationDirPath() + "/config.txt";
}

/*
 * readConfigFile —— 从 config.txt 读取 COM_PORT 和 BAUD_RATE
 *
 * 文件格式（简单 KEY=VALUE）：
 *   # 注释行
 *   COM_PORT=/dev/ttyTHS1
 *   BAUD_RATE=115200
 *
 * 返回值：true=读取成功, false=文件不存在或无有效配置
 */
bool MainWindow::readConfigFile(QString &comPort, int &baudRate) const
{
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    bool hasCom = false, hasBaud = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        // 跳过注释行和空行
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) {
            continue;
        }

        int eqIdx = line.indexOf('=');
        if (eqIdx < 0) continue;

        QString key = line.left(eqIdx).trimmed();
        QString value = line.mid(eqIdx + 1).trimmed();

        if (key.compare("COM_PORT", Qt::CaseInsensitive) == 0 && !value.isEmpty()) {
            comPort = value;
            hasCom = true;
        } else if (key.compare("BAUD_RATE", Qt::CaseInsensitive) == 0) {
            bool ok = false;
            int rate = value.toInt(&ok);
            if (ok && rate > 0) {
                baudRate = rate;
                hasBaud = true;
            }
        }
    }

    file.close();
    return hasCom && hasBaud;
}

/*
 * loadGnssConfig —— 从 config.txt 加载串口配置到 UI 控件
 *
 * 初始化时调用，将 config.txt 中的值恢复到 UI 下拉框。
 */
void MainWindow::loadGnssConfig()
{
    QString comPort;
    int baudRate = 115200;

    if (readConfigFile(comPort, baudRate)) {
        ui->comboGnssCom->setCurrentText(comPort);
        ui->comboGnssBaud->setCurrentText(QString::number(baudRate));
    } else {
        // 配置文件不存在时使用默认值
        ui->comboGnssCom->setCurrentText("/dev/ttyTHS1");
        ui->comboGnssBaud->setCurrentText("115200");
    }
}

/*
 * saveGnssConfig —— 将 UI 控件的串口配置写入 config.txt
 *
 * 写入格式：
 *   COM_PORT=<comboGnssCom 的值>
 *   BAUD_RATE=<comboGnssBaud 的值>
 */
void MainWindow::saveGnssConfig()
{
    const QString comPort = ui->comboGnssCom->currentText().trimmed();
    const QString baudRate = ui->comboGnssBaud->currentText().trimmed();

    if (comPort.isEmpty() || baudRate.isEmpty()) {
        return;
    }

    QFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << "COM_PORT=" << comPort << "\n";
    out << "BAUD_RATE=" << baudRate << "\n";
    file.close();
}

/*
 * 保存按钮：将当前 UI 串口设置写入配置文件
 */
void MainWindow::on_btnSaveGnssConfig_clicked()
{
    saveGnssConfig();

    // 保存后给出绿色文字提示
    ui->labelGnssStatus->setText("状态：配置已保存");
    ui->labelGnssStatus->setStyleSheet("font: 12px \"Microsoft YaHei\"; color: #1b8d42;");
}
