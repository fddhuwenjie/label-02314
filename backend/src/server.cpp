/**
 * @file server.cpp
 * @brief TCP服务器类实现文件
 * 
 * 实现聊天室服务器的核心功能。
 */

#include "server.h"
#include "logger.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

Server::Server(Database *db, QObject *parent)
    : QTcpServer(parent), m_db(db) {
    LOG_DEBUG("Server", "服务器对象已创建");
}

Server::~Server() {
    LOG_DEBUG("Server", "服务器对象已销毁");
}

/**
 * @brief 写入日志消息
 * 
 * 发出信号供UI显示，并通过全局 Logger 记录日志。
 */
void Server::writeLog(const QString &msg) {
    // 发出信号供UI显示
    emit logMessage(msg);
    
    // 使用全局日志系统
    LOG_INFO("Server", msg);
}

/**
 * @brief 启动服务器
 * 
 * 开始监听指定端口，等待客户端连接。
 * 包含异常处理以确保服务器稳定性。
 */
ServerResult Server::startServer(quint16 port) {
    ServerResult result{false, ""};
    
    try {
        if (!listen(QHostAddress::Any, port)) {
            result.errorMessage = QString("服务器启动失败: %1 (端口: %2)").arg(errorString()).arg(port);
            LOG_ERROR("Server", result.errorMessage);
            emit errorOccurred(result.errorMessage);
            return result;
        }
        
        LOG_INFO("Server", QString("服务器已启动，监听端口 %1").arg(port));
        writeLog(QString("服务器已启动，监听端口 %1").arg(port));
        result.success = true;
        
    } catch (const std::exception &e) {
        result.errorMessage = QString("服务器启动异常: %1").arg(e.what());
        LOG_ERROR("Server", result.errorMessage);
        emit errorOccurred(result.errorMessage);
    }
    
    return result;
}

/**
 * @brief 停止服务器
 * 
 * 断开所有客户端连接，停止监听端口。
 */
void Server::stopServer() {
    try {
        // 断开所有客户端连接
        for (auto socket : m_socketToUser.keys()) {
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                socket->disconnectFromHost();
            }
        }
        close();
        LOG_INFO("Server", "服务器已停止");
        writeLog("服务器已停止");
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("停止服务器异常: %1").arg(e.what()));
    }
}

void Server::setMaxOnlineUsers(int max) {
    m_maxOnline = max;
    LOG_DEBUG("Server", QString("最大在线人数设置为: %1").arg(max));
}

QList<OnlineUser> Server::getOnlineUsers() const {
    return m_onlineUsers.values();
}

/**
 * @brief 处理新的客户端连接
 * 
 * 为每个新连接创建 QTcpSocket，并连接信号槽。
 * 包含异常处理以防止单个连接错误影响服务器。
 */
void Server::incomingConnection(qintptr socketDescriptor) {
    try {
        QTcpSocket *socket = new QTcpSocket(this);
        
        if (!socket->setSocketDescriptor(socketDescriptor)) {
            LOG_ERROR("Server", QString("设置套接字描述符失败: %1").arg(socket->errorString()));
            delete socket;
            return;
        }
        
        // 连接信号槽
        connect(socket, &QTcpSocket::readyRead, this, &Server::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &Server::onDisconnected);
        
        // 处理套接字错误
        connect(socket, &QTcpSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError error) {
            Q_UNUSED(error)
            LOG_WARNING("Server", QString("套接字错误: %1").arg(socket->errorString()));
        });
        
        LOG_INFO("Server", QString("新连接: %1:%2")
                 .arg(socket->peerAddress().toString())
                 .arg(socket->peerPort()));
        writeLog(QString("新连接: %1:%2")
                 .arg(socket->peerAddress().toString())
                 .arg(socket->peerPort()));
                 
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("处理新连接异常: %1").arg(e.what()));
    }
}

/**
 * @brief 处理客户端数据到达
 * 
 * 读取客户端发送的数据并进行处理。
 */
void Server::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    try {
        QByteArray data = socket->readAll();
        processMessage(socket, data);
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("读取数据异常: %1").arg(e.what()));
    }
}

/**
 * @brief 处理客户端断开连接
 * 
 * 清理用户状态，从在线列表中移除，并广播更新。
 */
void Server::onDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    try {
        QString username = m_socketToUser.value(socket);
        if (!username.isEmpty()) {
            // 清理用户状态
            m_socketToUser.remove(socket);
            m_userToSocket.remove(username);
            m_onlineUsers.remove(username);
            
            emit userLoggedOut(username);
            LOG_INFO("Server", QString("用户 %1 已下线").arg(username));
            writeLog(QString("用户 %1 已下线").arg(username));
            
            // 广播更新的用户列表
            broadcastOnlineUsers();
        }
        socket->deleteLater();
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("处理断开连接异常: %1").arg(e.what()));
    }
}

/**
 * @brief 处理接收到的消息
 * 
 * 解析 JSON 格式的消息，根据消息类型分发到对应的处理函数。
 * 
 * 支持的消息类型：
 * - login: 用户登录
 * - register: 用户注册
 * - chat: 聊天消息
 * - get_online_users: 获取在线用户列表
 * - get_history: 获取历史消息
 */
void Server::processMessage(QTcpSocket *socket, const QByteArray &data) {
    try {
        // 解析 JSON 数据
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            LOG_WARNING("Server", QString("JSON解析错误: %1").arg(parseError.errorString()));
            return;
        }
        
        if (!doc.isObject()) {
            LOG_WARNING("Server", "收到的数据不是有效的JSON对象");
            return;
        }
        
        QJsonObject json = doc.object();
        QString type = json["type"].toString();
        
        // 根据消息类型分发处理
        if (type == "login") {
            handleLogin(socket, json);
        } else if (type == "register") {
            handleRegister(socket, json);
        } else if (type == "chat") {
            handleChat(socket, json);
        } else if (type == "get_online_users") {
            handleGetOnlineUsers(socket);
        } else if (type == "get_history") {
            handleGetHistory(socket, json);
        } else {
            LOG_WARNING("Server", QString("未知的消息类型: %1").arg(type));
        }
        
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("处理消息异常: %1").arg(e.what()));
    }
}

/**
 * @brief 处理用户登录请求
 * 
 * 验证流程：
 * 1. 校验输入参数（用户名和密码非空）
 * 2. 检查服务器是否已满
 * 3. 检查用户是否已在线
 * 4. 验证用户名和密码
 * 5. 登录成功后添加到在线列表并广播
 */
void Server::handleLogin(QTcpSocket *socket, const QJsonObject &json) {
    QString username = json["username"].toString().trimmed();
    QString password = json["password"].toString();
    
    QJsonObject response;
    response["type"] = "login_response";
    
    try {
        // 检查0: 输入参数校验
        if (username.isEmpty()) {
            response["success"] = false;
            response["message"] = "用户名不能为空";
            sendToClient(socket, response);
            LOG_WARNING("Server", "登录拒绝: 用户名为空");
            return;
        }
        
        if (password.isEmpty()) {
            response["success"] = false;
            response["message"] = "密码不能为空";
            sendToClient(socket, response);
            LOG_WARNING("Server", QString("登录拒绝: 密码为空 (用户: %1)").arg(username));
            return;
        }
        
        // 检查1: 服务器是否已满
        if (m_onlineUsers.size() >= m_maxOnline) {
            response["success"] = false;
            response["message"] = QString("服务器已满，最大允许 %1 人在线").arg(m_maxOnline);
            sendToClient(socket, response);
            LOG_WARNING("Server", QString("登录拒绝: 服务器已满 (用户: %1)").arg(username));
            writeLog(QString("登录拒绝: 服务器已满 (用户: %1)").arg(username));
            return;
        }
        
        // 检查2: 用户是否已在线
        if (m_userToSocket.contains(username)) {
            response["success"] = false;
            response["message"] = "该用户已在线";
            sendToClient(socket, response);
            LOG_WARNING("Server", QString("登录拒绝: 用户 %1 已在线").arg(username));
            writeLog(QString("登录拒绝: 用户 %1 已在线").arg(username));
            return;
        }
        
        // 检查3: 验证用户名和密码
        DbResult validateResult = m_db->validateUser(username, password);
        if (validateResult.success) {
            // 获取用户信息
            UserInfo info = m_db->getUserInfo(username);
            
            // 创建在线用户记录
            OnlineUser user;
            user.username = username;
            user.nickname = info.nickname;
            user.ip = socket->peerAddress().toString();
            user.port = socket->peerPort();
            user.loginTime = QDateTime::currentDateTime();
            user.role = info.role;
            
            // 添加到在线列表
            m_socketToUser[socket] = username;
            m_userToSocket[username] = socket;
            m_onlineUsers[username] = user;
            
            // 发送成功响应
            response["success"] = true;
            response["nickname"] = info.nickname;
            response["role"] = static_cast<int>(info.role);
            sendToClient(socket, response);
            
            emit userLoggedIn(user);
            LOG_INFO("Server", QString("用户 %1 登录成功 (IP: %2)").arg(username).arg(user.ip));
            writeLog(QString("用户 %1 登录成功 (IP: %2)").arg(username).arg(user.ip));
            
            // 广播更新的用户列表
            broadcastOnlineUsers();
        } else {
            response["success"] = false;
            response["message"] = validateResult.errorMessage;
            sendToClient(socket, response);
            LOG_WARNING("Server", QString("登录失败: %1 (用户: %2)").arg(validateResult.errorMessage).arg(username));
            writeLog(QString("登录失败: %1 (用户: %2)").arg(validateResult.errorMessage).arg(username));
        }
        
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("处理登录异常: %1").arg(e.what()));
        response["success"] = false;
        response["message"] = "服务器内部错误";
        sendToClient(socket, response);
    }
}

/**
 * @brief 处理用户注册请求
 * 
 * 验证流程：
 * 1. 校验输入参数（用户名、密码、昵称非空）
 * 2. 调用数据库的注册方法
 * 3. 返回注册结果
 */
void Server::handleRegister(QTcpSocket *socket, const QJsonObject &json) {
    QString username = json["username"].toString().trimmed();
    QString password = json["password"].toString();
    QString nickname = json["nickname"].toString().trimmed();
    
    QJsonObject response;
    response["type"] = "register_response";
    
    try {
        // 输入参数校验
        if (username.isEmpty()) {
            response["success"] = false;
            response["message"] = "用户名不能为空";
            sendToClient(socket, response);
            LOG_WARNING("Server", "注册拒绝: 用户名为空");
            return;
        }
        
        if (password.isEmpty()) {
            response["success"] = false;
            response["message"] = "密码不能为空";
            sendToClient(socket, response);
            LOG_WARNING("Server", QString("注册拒绝: 密码为空 (用户: %1)").arg(username));
            return;
        }
        
        if (nickname.isEmpty()) {
            response["success"] = false;
            response["message"] = "昵称不能为空";
            sendToClient(socket, response);
            LOG_WARNING("Server", QString("注册拒绝: 昵称为空 (用户: %1)").arg(username));
            return;
        }
        
        DbResult registerResult = m_db->registerUser(username, password, nickname);
        if (registerResult.success) {
            response["success"] = true;
            LOG_INFO("Server", QString("新用户注册: %1").arg(username));
            writeLog(QString("新用户注册: %1").arg(username));
        } else {
            response["success"] = false;
            response["message"] = registerResult.errorMessage;
            LOG_WARNING("Server", QString("注册失败: %1 (用户: %2)").arg(registerResult.errorMessage).arg(username));
            writeLog(QString("注册失败: %1 (用户: %2)").arg(registerResult.errorMessage).arg(username));
        }
        sendToClient(socket, response);
        
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("处理注册异常: %1").arg(e.what()));
        response["success"] = false;
        response["message"] = "服务器内部错误";
        sendToClient(socket, response);
    }
}

/**
 * @brief 处理聊天消息
 * 
 * 支持两种消息类型：
 * - 群聊：to="all"，消息发送给所有在线用户
 * - 私聊：to=用户名，消息只发送给指定用户
 * 
 * 所有消息都会保存到数据库。
 */
void Server::handleChat(QTcpSocket *socket, const QJsonObject &json) {
    QString from = m_socketToUser.value(socket);
    QString to = json["to"].toString();
    QString message = json["message"].toString();
    
    // 验证发送者是否已登录
    if (from.isEmpty()) {
        LOG_WARNING("Server", "聊天消息被拒绝: 发送者未登录");
        writeLog("聊天消息被拒绝: 发送者未登录");
        return;
    }
    
    try {
        // 构建聊天消息
        QJsonObject chatMsg;
        chatMsg["type"] = "chat";
        chatMsg["from"] = from;
        chatMsg["from_nickname"] = m_onlineUsers[from].nickname;
        chatMsg["message"] = message;
        chatMsg["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        
        bool isGroup = (to == "all");
        
        // 保存消息到数据库
        DbResult saveResult = m_db->saveMessage(from, to, message, isGroup);
        if (!saveResult.success) {
            LOG_WARNING("Server", QString("消息保存失败: %1").arg(saveResult.errorMessage));
        }
        
        if (isGroup) {
            // 群发消息给所有在线用户
            for (auto targetSocket : m_userToSocket.values()) {
                sendToClient(targetSocket, chatMsg);
            }
            emit messageReceived(from, "all", message);
            LOG_DEBUG("Server", QString("群发消息: %1 -> 全体").arg(from));
            writeLog(QString("群发消息: %1 -> 全体").arg(from));
        } else if (m_userToSocket.contains(to)) {
            // 私聊消息
            chatMsg["to"] = to;
            sendToClient(m_userToSocket[to], chatMsg);  // 发送给接收者
            sendToClient(socket, chatMsg);              // 发送给发送者（确认）
            emit messageReceived(from, to, message);
            LOG_DEBUG("Server", QString("私聊消息: %1 -> %2").arg(from).arg(to));
            writeLog(QString("私聊消息: %1 -> %2").arg(from).arg(to));
        } else {
            // 目标用户不在线
            QJsonObject errorResponse;
            errorResponse["type"] = "error";
            errorResponse["message"] = QString("用户 %1 不在线").arg(to);
            sendToClient(socket, errorResponse);
            LOG_DEBUG("Server", QString("私聊失败: 用户 %1 不在线").arg(to));
        }
        
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("处理聊天消息异常: %1").arg(e.what()));
    }
}

/**
 * @brief 处理获取在线用户列表请求
 * 
 * 返回当前所有在线用户的信息。
 */
void Server::handleGetOnlineUsers(QTcpSocket *socket) {
    try {
        QJsonObject response;
        response["type"] = "online_users";
        
        QJsonArray users;
        for (const auto &user : m_onlineUsers) {
            QJsonObject u;
            u["username"] = user.username;
            u["nickname"] = user.nickname;
            u["role"] = static_cast<int>(user.role);
            users.append(u);
        }
        response["users"] = users;
        sendToClient(socket, response);
        
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("获取在线用户列表异常: %1").arg(e.what()));
    }
}

/**
 * @brief 处理获取历史消息请求
 * 
 * 根据目标类型返回私聊或群聊的历史消息。
 * 
 * @param socket 请求的客户端套接字
 * @param json 包含 target（目标用户或"all"）和 limit（消息数量限制）
 */
void Server::handleGetHistory(QTcpSocket *socket, const QJsonObject &json) {
    QString from = m_socketToUser.value(socket);
    QString target = json["target"].toString();
    int limit = json["limit"].toInt(50);
    
    // 验证请求者是否已登录
    if (from.isEmpty()) {
        LOG_WARNING("Server", "历史记录请求被拒绝: 用户未登录");
        return;
    }
    
    try {
        QJsonObject response;
        response["type"] = "history_response";
        
        // 根据目标类型查询历史消息
        QList<MessageRecord> records;
        if (target == "all") {
            records = m_db->getGroupMessageHistory(limit);
        } else {
            records = m_db->getMessageHistory(from, target, limit);
        }
        
        // 构建消息数组
        QJsonArray messages;
        for (const auto &record : records) {
            QJsonObject msg;
            msg["from"] = record.fromUser;
            msg["to"] = record.toUser;
            msg["message"] = record.message;
            msg["time"] = record.timestamp;
            msg["is_group"] = record.isGroupMessage;
            messages.append(msg);
        }
        response["messages"] = messages;
        sendToClient(socket, response);
        
        LOG_DEBUG("Server", QString("历史记录查询: %1 -> %2 (共%3条)").arg(from).arg(target).arg(records.size()));
        writeLog(QString("历史记录查询: %1 -> %2 (共%3条)").arg(from).arg(target).arg(records.size()));
        
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("获取历史消息异常: %1").arg(e.what()));
    }
}

/**
 * @brief 向客户端发送 JSON 消息
 * 
 * 将 JSON 对象序列化后发送给指定客户端。
 * 包含连接状态检查以避免发送失败。
 */
void Server::sendToClient(QTcpSocket *socket, const QJsonObject &json) {
    try {
        if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
            LOG_DEBUG("Server", "发送失败: 套接字无效或未连接");
            return;
        }
        QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
        socket->write(data);
        socket->flush();
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("发送消息异常: %1").arg(e.what()));
    }
}

/**
 * @brief 广播在线用户列表
 * 
 * 向所有在线用户发送最新的用户列表。
 * 通常在用户登录或登出时调用。
 */
void Server::broadcastOnlineUsers() {
    try {
        QJsonObject response;
        response["type"] = "online_users";
        
        QJsonArray users;
        for (const auto &user : m_onlineUsers) {
            QJsonObject u;
            u["username"] = user.username;
            u["nickname"] = user.nickname;
            u["role"] = static_cast<int>(user.role);
            users.append(u);
        }
        response["users"] = users;
        
        // 发送给所有在线用户
        for (auto socket : m_userToSocket.values()) {
            sendToClient(socket, response);
        }
        
        LOG_DEBUG("Server", QString("广播在线用户列表，当前 %1 人在线").arg(m_onlineUsers.size()));
        
    } catch (const std::exception &e) {
        LOG_ERROR("Server", QString("广播用户列表异常: %1").arg(e.what()));
    }
}
