/*
 * mainwindow.h —— 主窗口声明
 *
 * 主窗口管理 4 个功能模块的启停和数据显示：
 *   - GNSS 模块（真实数据）：NeoM8pReceiver → fixUpdated 信号 → UI
 *   - INS 模块（模拟数据）：updateSensorData 定时器生成三角函数模拟值
 *   - 图像模块（模拟视频）：updateImageData 定时器 + QPainter 绘制
 *
 * 定时器：
 *   - m_sensorTimer (500ms) : 驱动 INS 模拟数据更新
 *   - m_imageTimer  (40ms)  : 驱动两路模拟图像刷新
 *     GNSS 不走定时器，由 NeoM8pReceiver 信号异步驱动
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QLabel;

// GNSS 模块（前向声明，避免头文件依赖扩散）
namespace gnss {
class NeoM8pReceiver;
struct GnssFix;
struct NeoM8pConfig;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    /* ── 按钮响应 ── 四路模块的启停按钮 ──────────── */

    /** @brief GNSS 模块开关：通过 NeoM8pReceiver 启停串口轮询 */
    void on_btnGnss_clicked();

    /** @brief INS 模块开关：启停模拟传感器数据生成 */
    void on_btnIns_clicked();

    /** @brief 图像第 1 路开关 */
    void on_btnImage1_clicked();

    /** @brief 图像第 2 路开关 */
    void on_btnImage2_clicked();

    /** @brief 保存 GNSS 串口配置到 config.txt */
    void on_btnSaveGnssConfig_clicked();

    /* ── 定时器 ──────────────────────────────────── */

    /** @brief 传感器定时器回调（500ms）：更新 INS 模拟数据 */
    void updateSensorData();

    /** @brief 图像定时器回调（40ms）：更新两路模拟图像 */
    void updateImageData();

    /* ── GNSS 信号响应 ────────────────────────────── */

    /** @brief 接收到有效的 GNSS 定位结果 */
    void onGnssFixUpdated(const gnss::GnssFix &fix);

    /** @brief GNSS 模块发生错误 */
    void onGnssError(const QString &message);

private:
    /* ── UI 辅助方法 ─────────────────────────────── */

    /** @brief 设置模块状态指示标签（开启/关闭 + 颜色） */
    void setButtonState(bool running, QLabel *statusLabel);

    /** @brief 重置 GNSS 显示为默认文本 */
    void resetGnssText();

    /** @brief 重置 INS 显示为默认文本 */
    void resetInsText();

    /** @brief 绘制一路模拟图像（网格 + 移动圆圈 + 帧号/时间戳） */
    void drawImage(QLabel *label, int channel);

    /** @brief 根据模块状态智能启停定时器（有任一模块运行则启动） */
    void updateTimers();

    /* ── 配置文件 ─────────────────────────────────── */

    /** @brief 获取 config.txt 的完整路径 */
    QString configFilePath() const;

    /** @brief 从 config.txt 解析 COM_PORT 和 BAUD_RATE，返回是否成功 */
    bool readConfigFile(QString &comPort, int &baudRate) const;

    /** @brief 从 config.txt 加载串口配置到 UI 控件 */
    void loadGnssConfig();

    /** @brief 从 UI 控件保存串口配置到 config.txt */
    void saveGnssConfig();

private:
    /* ── 成员变量 ─────────────────────────────────── */

    Ui::MainWindow *ui;

    // 定时器
    QTimer m_sensorTimer;   // 传感器数据定时器（500ms）
    QTimer m_imageTimer;    // 图像刷新定时器（40ms）

    // GNSS 接收机（通过 DLL → 串口获取真实数据）
    gnss::NeoM8pReceiver *m_gnssReceiver = nullptr;

    // 模块运行状态标记
    bool m_gnssRunning = false;
    bool m_insRunning = false;
    bool m_image1Running = false;
    bool m_image2Running = false;

    // 模拟数据状态
    double m_tick = 0.0;        // INS 模拟时间累加器（秒）
    int m_frameNo = 0;          // 图像帧序号
};

#endif // MAINWINDOW_H
