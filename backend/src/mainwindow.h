/**
 * @file mainwindow.h
 * @brief 主窗口类头文件
 * 
 * 提供服务器管理的图形界面，包括：
 * - 服务器启动/停止控制
 * - 在线用户列表显示
 * - 服务器日志显示
 * - 最大在线人数设置
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QTextEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include "server.h"
#include "database.h"

/**
 * @class MainWindow
 * @brief 服务器管理主窗口
 * 
 * 提供完整的服务器管理界面，包括：
 * - 服务器控制面板（启动/停止、端口设置、最大人数）
 * - 在线用户列表（实时更新）
 * - 服务器日志显示（带颜色区分错误）
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param db 数据库对象指针
     * @param parent 父窗口指针
     */
    explicit MainWindow(Database *db, QWidget *parent = nullptr);

    /**
     * @brief 析构函数，停止服务器
     */
    ~MainWindow();

private slots:
    /**
     * @brief 启动服务器按钮点击处理
     */
    void onStartServer();

    /**
     * @brief 停止服务器按钮点击处理
     */
    void onStopServer();

    /**
     * @brief 用户登录事件处理
     * @param user 登录的用户信息
     */
    void onUserLoggedIn(const OnlineUser &user);

    /**
     * @brief 用户登出事件处理
     * @param username 登出的用户名
     */
    void onUserLoggedOut(const QString &username);

    /**
     * @brief 日志消息处理
     * @param msg 日志内容
     */
    void onLogMessage(const QString &msg);

    /**
     * @brief 最大在线人数变更处理
     * @param value 新的最大人数
     */
    void onMaxOnlineChanged(int value);

    /**
     * @brief 错误发生处理
     * @param error 错误信息
     */
    void onErrorOccurred(const QString &error);

private:
    /**
     * @brief 初始化用户界面
     */
    void setupUI();

    /**
     * @brief 更新在线用户表格
     */
    void updateOnlineTable();

    Database *m_db;              ///< 数据库对象指针
    Server *m_server;            ///< 服务器对象指针
    
    // UI 组件
    QTableWidget *m_onlineTable; ///< 在线用户表格
    QTextEdit *m_logText;        ///< 日志显示区域
    QSpinBox *m_portSpin;        ///< 端口号输入框
    QSpinBox *m_maxOnlineSpin;   ///< 最大在线人数输入框
    QPushButton *m_startBtn;     ///< 启动按钮
    QPushButton *m_stopBtn;      ///< 停止按钮
    QLabel *m_statusLabel;       ///< 状态标签
    QLabel *m_onlineCountLabel;  ///< 在线人数标签
};

#endif // MAINWINDOW_H
