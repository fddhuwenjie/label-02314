/**
 * @file server.h
 * @brief TCP服务器类头文件
 * 
 * 提供聊天室服务器的核心功能，包括：
 * - TCP连接管理
 * - 用户登录/注册处理
 * - 消息转发（私聊/群聊）
 * - 在线用户管理
 * - 日志记录
 */

#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QJsonObject>
#include "database.h"

/**
 * @struct OnlineUser
 * @brief 在线用户信息结构体
 * 
 * 存储当前在线用户的详细信息，用于用户管理和显示。
 */
struct OnlineUser {
    QString username;       ///< 用户名
    QString nickname;       ///< 昵称
    QString ip;             ///< 客户端IP地址
    quint16 port;           ///< 客户端端口号
    QDateTime loginTime;    ///< 登录时间
    UserRole role;          ///< 用户角色
};

/**
 * @struct ServerResult
 * @brief 服务器操作结果结构体
 * 
 * 用于返回服务器操作的执行结果。
 */
struct ServerResult {
    bool success;           ///< 操作是否成功
    QString errorMessage;   ///< 错误信息（失败时有效）
};

/**
 * @class Server
 * @brief TCP聊天服务器类
 * 
 * 继承自 QTcpServer，实现完整的聊天室服务器功能：
 * 
 * 核心功能：
 * - 监听指定端口，接受客户端连接
 * - 处理用户登录、注册请求
 * - 转发聊天消息（支持私聊和群聊）
 * - 管理在线用户列表
 * - 记录服务器日志
 * 
 * 通信协议：
 * - 使用 JSON 格式进行数据交换
 * - 支持的消息类型：login, register, chat, get_online_users, get_history
 * 
 * 使用示例：
 * @code
 * Database db;
 * db.init("sqlite", "", 0, "chat.db", "", "");
 * 
 * Server server(&db);
 * server.setMaxOnlineUsers(100);
 * server.setLogFile("/path/to/server.log");
 * server.enableLogPersistence(true);
 * 
 * ServerResult result = server.startServer(9999);
 * if (result.success) {
 *     // 服务器启动成功
 * }
 * @endcode
 */
class Server : public QTcpServer {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param db 数据库对象指针
     * @param parent 父对象指针
     */
    explicit Server(Database *db, QObject *parent = nullptr);

    /**
     * @brief 析构函数，清理资源
     */
    ~Server();
    
    /**
     * @brief 启动服务器
     * @param port 监听端口号
     * @return ServerResult 启动结果
     * 
     * 开始监听指定端口，等待客户端连接。
     */
    ServerResult startServer(quint16 port);

    /**
     * @brief 停止服务器
     * 
     * 断开所有客户端连接，停止监听。
     */
    void stopServer();

    /**
     * @brief 设置最大在线人数
     * @param max 最大在线人数限制
     */
    void setMaxOnlineUsers(int max);

    /**
     * @brief 获取最大在线人数设置
     * @return int 最大在线人数
     */
    int maxOnlineUsers() const { return m_maxOnline; }

    /**
     * @brief 获取当前在线用户列表
     * @return QList<OnlineUser> 在线用户列表
     */
    QList<OnlineUser> getOnlineUsers() const;

signals:
    /**
     * @brief 用户登录信号
     * @param user 登录的用户信息
     */
    void userLoggedIn(const OnlineUser &user);

    /**
     * @brief 用户登出信号
     * @param username 登出的用户名
     */
    void userLoggedOut(const QString &username);

    /**
     * @brief 消息接收信号
     * @param from 发送者用户名
     * @param to 接收者用户名（群聊时为"all"）
     * @param msg 消息内容
     */
    void messageReceived(const QString &from, const QString &to, const QString &msg);

    /**
     * @brief 日志消息信号
     * @param msg 日志内容
     */
    void logMessage(const QString &msg);

    /**
     * @brief 错误发生信号
     * @param error 错误信息
     */
    void errorOccurred(const QString &error);

protected:
    /**
     * @brief 处理新的客户端连接
     * @param socketDescriptor 套接字描述符
     * 
     * 重写 QTcpServer 的虚函数，为每个新连接创建 QTcpSocket。
     */
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    /**
     * @brief 处理客户端数据到达
     * 
     * 当客户端发送数据时触发，读取并处理消息。
     */
    void onReadyRead();

    /**
     * @brief 处理客户端断开连接
     * 
     * 清理用户状态，广播用户列表更新。
     */
    void onDisconnected();

private:
    /**
     * @brief 处理接收到的消息
     * @param socket 客户端套接字
     * @param data 接收到的原始数据
     * 
     * 解析 JSON 消息并分发到对应的处理函数。
     */
    void processMessage(QTcpSocket *socket, const QByteArray &data);

    /**
     * @brief 处理登录请求
     * @param socket 客户端套接字
     * @param json 登录请求的 JSON 数据
     */
    void handleLogin(QTcpSocket *socket, const QJsonObject &json);

    /**
     * @brief 处理注册请求
     * @param socket 客户端套接字
     * @param json 注册请求的 JSON 数据
     */
    void handleRegister(QTcpSocket *socket, const QJsonObject &json);

    /**
     * @brief 处理聊天消息
     * @param socket 客户端套接字
     * @param json 聊天消息的 JSON 数据
     */
    void handleChat(QTcpSocket *socket, const QJsonObject &json);

    /**
     * @brief 处理获取在线用户列表请求
     * @param socket 客户端套接字
     */
    void handleGetOnlineUsers(QTcpSocket *socket);

    /**
     * @brief 处理获取历史消息请求
     * @param socket 客户端套接字
     * @param json 请求的 JSON 数据
     */
    void handleGetHistory(QTcpSocket *socket, const QJsonObject &json);

    /**
     * @brief 向客户端发送消息
     * @param socket 目标客户端套接字
     * @param json 要发送的 JSON 数据
     */
    void sendToClient(QTcpSocket *socket, const QJsonObject &json);

    /**
     * @brief 广播在线用户列表
     * 
     * 向所有在线用户发送最新的用户列表。
     */
    void broadcastOnlineUsers();

    /**
     * @brief 写入日志
     * @param msg 日志消息
     */
    void writeLog(const QString &msg);

    Database *m_db;                              ///< 数据库对象指针
    int m_maxOnline = 100;                       ///< 最大在线人数
    QMap<QTcpSocket*, QString> m_socketToUser;  ///< 套接字到用户名的映射
    QMap<QString, QTcpSocket*> m_userToSocket;  ///< 用户名到套接字的映射
    QMap<QString, OnlineUser> m_onlineUsers;    ///< 在线用户信息映射
};

#endif // SERVER_H
