/**
 * @file mainwindow.cpp
 * @brief 主窗口类实现文件
 */

#include "mainwindow.h"
#include "logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QDateTime>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>

MainWindow::MainWindow(Database *db, QWidget *parent)
    : QMainWindow(parent), m_db(db) {
    
    // 创建服务器实例
    m_server = new Server(db, this);
    
    // 连接服务器信号
    connect(m_server, &Server::userLoggedIn, this, &MainWindow::onUserLoggedIn);
    connect(m_server, &Server::userLoggedOut, this, &MainWindow::onUserLoggedOut);
    connect(m_server, &Server::logMessage, this, &MainWindow::onLogMessage);
    connect(m_server, &Server::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_db, &Database::errorOccurred, this, &MainWindow::onErrorOccurred);
    
    // 设置默认日志文件路径（使用全局 Logger）
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString logPath = logDir + "/server.log";
    Logger::instance()->setLogFile(logPath);
    Logger::instance()->enablePersistence(true);
    
    setupUI();
    setWindowTitle("聊天室服务器管理");
    resize(800, 600);
    
    LOG_INFO("MainWindow", "主窗口初始化完成");
}

MainWindow::~MainWindow() {
    m_server->stopServer();
    LOG_INFO("MainWindow", "主窗口已关闭");
}

/**
 * @brief 初始化用户界面
 * 
 * 创建三个主要区域：
 * 1. 控制面板 - 服务器启动/停止、端口和人数设置
 * 2. 在线用户列表 - 显示当前在线的用户信息
 * 3. 日志区域 - 显示服务器运行日志
 */
void MainWindow::setupUI() {
    QWidget *central = new QWidget();
    setCentralWidget(central);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    
    // ========== 控制面板 ==========
    QGroupBox *controlGroup = new QGroupBox("服务器控制");
    QHBoxLayout *controlLayout = new QHBoxLayout(controlGroup);
    
    // 端口设置
    controlLayout->addWidget(new QLabel("端口:"));
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(9999);
    controlLayout->addWidget(m_portSpin);
    
    // 最大在线人数设置
    controlLayout->addWidget(new QLabel("最大在线人数:"));
    m_maxOnlineSpin = new QSpinBox();
    m_maxOnlineSpin->setRange(1, 1000);
    m_maxOnlineSpin->setValue(100);
    connect(m_maxOnlineSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MainWindow::onMaxOnlineChanged);
    controlLayout->addWidget(m_maxOnlineSpin);
    
    // 启动按钮
    m_startBtn = new QPushButton("启动服务器");
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartServer);
    controlLayout->addWidget(m_startBtn);
    
    // 停止按钮
    m_stopBtn = new QPushButton("停止服务器");
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopServer);
    controlLayout->addWidget(m_stopBtn);
    
    // 状态标签
    m_statusLabel = new QLabel("状态: 未启动");
    m_statusLabel->setStyleSheet("color: gray;");
    controlLayout->addWidget(m_statusLabel);
    
    controlLayout->addStretch();
    mainLayout->addWidget(controlGroup);

    // ========== 在线用户列表 ==========
    QGroupBox *onlineGroup = new QGroupBox("在线用户");
    QVBoxLayout *onlineLayout = new QVBoxLayout(onlineGroup);
    
    m_onlineCountLabel = new QLabel("当前在线: 0 人");
    onlineLayout->addWidget(m_onlineCountLabel);
    
    m_onlineTable = new QTableWidget();
    m_onlineTable->setColumnCount(5);
    m_onlineTable->setHorizontalHeaderLabels({"用户名", "昵称", "IP地址", "端口", "登录时间"});
    m_onlineTable->horizontalHeader()->setStretchLastSection(true);
    m_onlineTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_onlineTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    onlineLayout->addWidget(m_onlineTable);
    
    mainLayout->addWidget(onlineGroup);
    
    // ========== 日志区域 ==========
    QGroupBox *logGroup = new QGroupBox("服务器日志");
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    
    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(150);
    logLayout->addWidget(m_logText);
    
    mainLayout->addWidget(logGroup);
}

/**
 * @brief 启动服务器
 * 
 * 获取端口和最大人数设置，启动服务器。
 * 成功后更新UI状态。
 */
void MainWindow::onStartServer() {
    quint16 port = m_portSpin->value();
    m_server->setMaxOnlineUsers(m_maxOnlineSpin->value());
    
    LOG_INFO("MainWindow", QString("尝试启动服务器，端口: %1").arg(port));
    
    ServerResult result = m_server->startServer(port);
    if (result.success) {
        // 更新UI状态
        m_startBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
        m_portSpin->setEnabled(false);
        m_statusLabel->setText("状态: 运行中");
        m_statusLabel->setStyleSheet("color: green;");
        LOG_INFO("MainWindow", "服务器启动成功");
    } else {
        LOG_ERROR("MainWindow", QString("服务器启动失败: %1").arg(result.errorMessage));
        QMessageBox::critical(this, "服务器启动失败", 
            QString("无法启动服务器:\n\n%1\n\n请检查端口是否被占用或权限是否足够。")
            .arg(result.errorMessage));
    }
}

/**
 * @brief 停止服务器
 * 
 * 停止服务器并重置UI状态。
 */
void MainWindow::onStopServer() {
    LOG_INFO("MainWindow", "停止服务器");
    m_server->stopServer();
    
    // 重置UI状态
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_portSpin->setEnabled(true);
    m_statusLabel->setText("状态: 已停止");
    m_statusLabel->setStyleSheet("color: gray;");
    m_onlineTable->setRowCount(0);
    m_onlineCountLabel->setText("当前在线: 0 人");
}

/**
 * @brief 处理用户登录事件
 * 
 * 更新在线用户表格。
 */
void MainWindow::onUserLoggedIn(const OnlineUser &user) {
    Q_UNUSED(user)
    updateOnlineTable();
}

/**
 * @brief 处理用户登出事件
 * 
 * 更新在线用户表格。
 */
void MainWindow::onUserLoggedOut(const QString &username) {
    Q_UNUSED(username)
    updateOnlineTable();
}

/**
 * @brief 处理日志消息
 * 
 * 在日志区域显示消息，带时间戳。
 */
void MainWindow::onLogMessage(const QString &msg) {
    QString time = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ");
    m_logText->append(time + msg);
}

/**
 * @brief 处理最大在线人数变更
 */
void MainWindow::onMaxOnlineChanged(int value) {
    m_server->setMaxOnlineUsers(value);
    LOG_DEBUG("MainWindow", QString("最大在线人数已更新为: %1").arg(value));
}

/**
 * @brief 更新在线用户表格
 * 
 * 从服务器获取在线用户列表并更新表格显示。
 */
void MainWindow::updateOnlineTable() {
    QList<OnlineUser> users = m_server->getOnlineUsers();
    m_onlineTable->setRowCount(users.size());
    m_onlineCountLabel->setText(QString("当前在线: %1 人").arg(users.size()));
    
    for (int i = 0; i < users.size(); ++i) {
        const OnlineUser &u = users[i];
        m_onlineTable->setItem(i, 0, new QTableWidgetItem(u.username));
        m_onlineTable->setItem(i, 1, new QTableWidgetItem(u.nickname));
        m_onlineTable->setItem(i, 2, new QTableWidgetItem(u.ip));
        m_onlineTable->setItem(i, 3, new QTableWidgetItem(QString::number(u.port)));
        m_onlineTable->setItem(i, 4, new QTableWidgetItem(u.loginTime.toString("yyyy-MM-dd hh:mm:ss")));
    }
}

/**
 * @brief 处理错误事件
 * 
 * 在日志区域以红色显示错误信息。
 */
void MainWindow::onErrorOccurred(const QString &error) {
    QString time = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ");
    m_logText->append(QString("<span style='color:red;'>%1错误: %2</span>").arg(time).arg(error));
    LOG_ERROR("MainWindow", error);
}
