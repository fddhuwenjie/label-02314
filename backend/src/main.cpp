/**
 * @file main.cpp
 * @brief 聊天室服务器主程序入口
 * 
 * 程序启动流程：
 * 1. 初始化日志系统
 * 2. 初始化数据库连接
 * 3. 显示登录窗口（GUI模式）或直接启动服务器（无头模式）
 * 4. 登录成功后显示主窗口
 * 
 * 环境变量：
 * - HEADLESS=1: 启用无头模式，跳过登录直接启动服务器
 * - SERVER_PORT: 服务器监听端口（默认9999）
 */

#include <QApplication>
#include <QCoreApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include "database.h"
#include "server.h"
#include "loginwindow.h"
#include "mainwindow.h"
#include "logger.h"

int main(int argc, char *argv[]) {
    // 检查是否为无头模式
    bool headless = qEnvironmentVariable("HEADLESS", "0") == "1";
    
    // 输出启动信息到标准输出
    fprintf(stdout, "ChatServer starting... HEADLESS=%s\n", headless ? "true" : "false");
    fflush(stdout);
    
    // 根据模式创建不同的应用对象
    QCoreApplication *app;
    if (headless) {
        app = new QCoreApplication(argc, argv);
    } else {
        QApplication *guiApp = new QApplication(argc, argv);
        guiApp->setStyle(QStyleFactory::create("Fusion"));
        app = guiApp;
    }
    
    // ========== 初始化日志系统 ==========
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (logDir.isEmpty()) {
        logDir = "/app/data";
    }
    QString logPath = logDir + "/chatserver.log";
    
    Logger::instance()->setLogFile(logPath);
    Logger::instance()->enablePersistence(true);
    Logger::instance()->setMinLevel(LogLevel::DEBUG);
    
    LOG_INFO("Main", "========== 聊天室服务器启动 ==========");
    LOG_INFO("Main", QString("运行模式: %1").arg(headless ? "无头模式" : "GUI模式"));
    LOG_INFO("Main", QString("日志文件: %1").arg(logPath));
    
    // ========== 初始化数据库 ==========
    Database db;
    QString dbType = qEnvironmentVariable("DB_TYPE", "sqlite");
    QString dbHost = qEnvironmentVariable("DB_HOST", "localhost");
    int dbPort = qEnvironmentVariable("DB_PORT", "3306").toInt();
    QString dbName = qEnvironmentVariable("DB_NAME", "chatserver");
    QString dbUser = qEnvironmentVariable("DB_USER", "root");
    QString dbPass = qEnvironmentVariable("DB_PASS", "");
    
    LOG_INFO("Main", QString("数据库类型: %1").arg(dbType));
    
    DbResult initResult = db.init(dbType, dbHost, dbPort, dbName, dbUser, dbPass);
    if (!initResult.success) {
        LOG_ERROR("Main", QString("数据库初始化失败: %1").arg(initResult.errorMessage));
        if (!headless) {
            QMessageBox::critical(nullptr, "数据库初始化失败",
                QString("无法连接到数据库:\n\n%1\n\n请检查数据库配置是否正确。")
                .arg(initResult.errorMessage));
        }
        delete app;
        return -1;
    }
    
    LOG_INFO("Main", "数据库初始化成功");
    
    int result;
    
    if (headless) {
        // ========== 无头模式：直接启动服务器 ==========
        Server server(&db);
        quint16 port = qEnvironmentVariable("SERVER_PORT", "9999").toUInt();
        int maxOnline = qEnvironmentVariable("MAX_ONLINE", "100").toInt();
        
        server.setMaxOnlineUsers(maxOnline);
        
        ServerResult startResult = server.startServer(port);
        if (!startResult.success) {
            fprintf(stderr, "Server start failed: %s\n", startResult.errorMessage.toUtf8().constData());
            LOG_ERROR("Main", QString("服务器启动失败: %1").arg(startResult.errorMessage));
            delete app;
            return -1;
        }
        
        fprintf(stdout, "Server started on port %d, max online: %d\n", port, maxOnline);
        fflush(stdout);
        LOG_INFO("Main", QString("服务器已启动，监听端口 %1，最大在线 %2 人").arg(port).arg(maxOnline));
        
        result = app->exec();
    } else {
        // ========== GUI模式：显示登录窗口 ==========
        LoginWindow loginWindow(&db);
        MainWindow mainWindow(&db);
        
        QObject::connect(&loginWindow, &LoginWindow::loginSuccess, [&]() {
            LOG_INFO("Main", "管理员登录成功，进入主界面");
            loginWindow.hide();
            mainWindow.show();
        });
        
        loginWindow.show();
        LOG_INFO("Main", "显示登录窗口");
        
        result = app->exec();
    }
    
    LOG_INFO("Main", "========== 聊天室服务器关闭 ==========");
    delete app;
    return result;
}
