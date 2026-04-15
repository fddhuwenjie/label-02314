/**
 * @file database.cpp
 * @brief 数据库操作类实现文件
 * 
 * 实现数据库连接、用户管理、消息存储等核心功能。
 */

#include "database.h"
#include "logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRandomGenerator>

Database::Database(QObject *parent) : QObject(parent) {
    LOG_DEBUG("Database", "数据库对象已创建");
}

Database::~Database() {
    if (m_db.isOpen()) {
        m_db.close();
        LOG_INFO("Database", "数据库连接已关闭");
    }
}

/**
 * @brief 生成16字节的随机盐值
 * 
 * 使用 Qt 的安全随机数生成器生成盐值，
 * 返回32字符的十六进制字符串。
 */
QString Database::generateSalt() {
    QByteArray salt;
    for (int i = 0; i < 16; ++i) {
        salt.append(static_cast<char>(QRandomGenerator::global()->bounded(256)));
    }
    return salt.toHex();
}

/**
 * @brief 对密码进行安全哈希处理
 * 
 * 使用 PBKDF2 风格的多轮哈希：
 * 1. 将密码与盐值拼接
 * 2. 使用 SHA-256 进行 10000 轮迭代
 * 3. 返回最终哈希的十六进制表示
 * 
 * @param password 明文密码
 * @param salt 盐值
 * @return QString 哈希后的密码
 */
QString Database::hashPassword(const QString &password, const QString &salt) {
    QByteArray data = (password + salt).toUtf8();
    // 使用多轮哈希增强安全性，防止彩虹表攻击
    for (int i = 0; i < 10000; ++i) {
        data = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    }
    return QString(data.toHex());
}

/**
 * @brief 验证密码复杂度
 * 
 * 检查密码是否满足以下要求：
 * - 长度至少8位
 * - 包含至少一个大写字母
 * - 包含至少一个小写字母
 * - 包含至少一个数字
 * - 包含至少一个特殊字符
 */
PasswordValidation Database::validatePasswordComplexity(const QString &password) {
    PasswordValidation result{true, ""};
    
    // 检查密码长度
    if (password.length() < 8) {
        result.valid = false;
        result.errorMessage = "密码长度至少8位";
        return result;
    }
    
    // 检查字符类型
    bool hasUpper = false, hasLower = false, hasDigit = false, hasSpecial = false;
    for (const QChar &c : password) {
        if (c.isUpper()) hasUpper = true;
        else if (c.isLower()) hasLower = true;
        else if (c.isDigit()) hasDigit = true;
        else hasSpecial = true;
    }
    
    // 返回具体的错误提示
    if (!hasUpper) {
        result.valid = false;
        result.errorMessage = "密码需包含大写字母";
    } else if (!hasLower) {
        result.valid = false;
        result.errorMessage = "密码需包含小写字母";
    } else if (!hasDigit) {
        result.valid = false;
        result.errorMessage = "密码需包含数字";
    } else if (!hasSpecial) {
        result.valid = false;
        result.errorMessage = "密码需包含特殊字符";
    }
    
    return result;
}

/**
 * @brief 初始化数据库连接
 * 
 * 根据指定的数据库类型建立连接，并创建必要的数据表。
 * 支持 SQLite、MySQL、PostgreSQL 三种数据库。
 * 
 * @throws DatabaseException 当驱动不可用或连接失败时
 */
DbResult Database::init(const QString &type, const QString &host, int port,
                        const QString &dbName, const QString &user, const QString &pass) {
    DbResult result{false, ""};
    
    LOG_INFO("Database", QString("正在初始化数据库连接 (类型: %1)").arg(type));
    
    try {
        // 根据类型选择数据库驱动
        QString driver = type.toLower() == "mysql" ? "QMYSQL" : 
                         type.toLower() == "postgresql" ? "QPSQL" : "QSQLITE";
        
        // 检查驱动是否可用
        if (!QSqlDatabase::isDriverAvailable(driver)) {
            QString errorMsg = QString("数据库驱动 %1 不可用").arg(driver);
            LOG_ERROR("Database", errorMsg);
            result.errorMessage = errorMsg;
            m_lastError = errorMsg;
            emit errorOccurred(errorMsg);
            throw DatabaseException(errorMsg, 1001);
        }
        
        m_db = QSqlDatabase::addDatabase(driver);
        
        // 配置数据库连接参数
        if (driver == "QSQLITE") {
            // SQLite 只需要文件路径
            m_db.setDatabaseName(dbName.isEmpty() ? "chatserver.db" : dbName);
            LOG_DEBUG("Database", QString("SQLite 数据库文件: %1").arg(m_db.databaseName()));
        } else {
            // MySQL/PostgreSQL 需要完整的连接信息
            m_db.setHostName(host);
            m_db.setPort(port);
            m_db.setDatabaseName(dbName);
            m_db.setUserName(user);
            m_db.setPassword(pass);
            LOG_DEBUG("Database", QString("连接到 %1:%2/%3").arg(host).arg(port).arg(dbName));
        }

        // 尝试打开数据库连接
        if (!m_db.open()) {
            QString errorMsg = QString("数据库连接失败: %1").arg(m_db.lastError().text());
            LOG_ERROR("Database", errorMsg);
            result.errorMessage = errorMsg;
            m_lastError = errorMsg;
            emit errorOccurred(errorMsg);
            throw DatabaseException(errorMsg, 1002);
        }
        
        LOG_INFO("Database", "数据库连接成功");

        // 创建数据表
        if (!createTables()) {
            QString errorMsg = QString("创建数据表失败: %1").arg(m_lastError);
            LOG_ERROR("Database", errorMsg);
            result.errorMessage = errorMsg;
            emit errorOccurred(errorMsg);
            throw DatabaseException(errorMsg, 1003);
        }
        
        LOG_INFO("Database", "数据库初始化完成");
        result.success = true;
        
    } catch (const DatabaseException &e) {
        // 异常已经被记录，直接返回结果
        result.success = false;
        result.errorMessage = e.message();
    } catch (const std::exception &e) {
        // 捕获其他标准异常
        QString errorMsg = QString("数据库初始化异常: %1").arg(e.what());
        LOG_ERROR("Database", errorMsg);
        result.errorMessage = errorMsg;
        m_lastError = errorMsg;
        emit errorOccurred(errorMsg);
    }
    
    return result;
}

/**
 * @brief 创建数据库表
 * 
 * 创建系统所需的三张表：
 * 1. admins - 管理员表（存储管理员账号）
 * 2. users - 用户表（存储普通用户信息）
 * 3. messages - 消息表（存储聊天记录）
 * 
 * 同时会插入默认管理员账号（admin/Admin@123）
 * 
 * @return bool 是否成功创建所有表
 */
bool Database::createTables() {
    QSqlQuery query(m_db);
    
    LOG_DEBUG("Database", "开始创建数据表...");
    
    try {
        // ========== 创建管理员表 ==========
        QString adminTable = R"(
            CREATE TABLE IF NOT EXISTS admins (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username VARCHAR(50) UNIQUE NOT NULL,
                password VARCHAR(128) NOT NULL,
                salt VARCHAR(32) NOT NULL
            )
        )";
        
        // 适配不同数据库的语法差异
        if (m_db.driverName() == "QMYSQL") {
            adminTable.replace("AUTOINCREMENT", "AUTO_INCREMENT");
        } else if (m_db.driverName() == "QPSQL") {
            adminTable.replace("INTEGER PRIMARY KEY AUTOINCREMENT", "SERIAL PRIMARY KEY");
        }
        
        if (!query.exec(adminTable)) {
            m_lastError = query.lastError().text();
            LOG_ERROR("Database", QString("创建admins表失败: %1").arg(m_lastError));
            return false;
        }
        LOG_DEBUG("Database", "admins表创建成功");

        // ========== 创建用户表 ==========
        QString userTable = R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username VARCHAR(50) UNIQUE NOT NULL,
                password VARCHAR(128) NOT NULL,
                salt VARCHAR(32) NOT NULL,
                nickname VARCHAR(50) NOT NULL,
                role INTEGER DEFAULT 0,
                register_time DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        if (m_db.driverName() == "QMYSQL") {
            userTable.replace("AUTOINCREMENT", "AUTO_INCREMENT");
        } else if (m_db.driverName() == "QPSQL") {
            userTable.replace("INTEGER PRIMARY KEY AUTOINCREMENT", "SERIAL PRIMARY KEY");
        }
        
        if (!query.exec(userTable)) {
            m_lastError = query.lastError().text();
            LOG_ERROR("Database", QString("创建users表失败: %1").arg(m_lastError));
            return false;
        }
        LOG_DEBUG("Database", "users表创建成功");
        
        // ========== 创建消息历史表 ==========
        QString messageTable = R"(
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                from_user VARCHAR(50) NOT NULL,
                to_user VARCHAR(50),
                message TEXT NOT NULL,
                is_group INTEGER DEFAULT 0,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        if (m_db.driverName() == "QMYSQL") {
            messageTable.replace("AUTOINCREMENT", "AUTO_INCREMENT");
        } else if (m_db.driverName() == "QPSQL") {
            messageTable.replace("INTEGER PRIMARY KEY AUTOINCREMENT", "SERIAL PRIMARY KEY");
        }
        
        if (!query.exec(messageTable)) {
            m_lastError = query.lastError().text();
            LOG_ERROR("Database", QString("创建messages表失败: %1").arg(m_lastError));
            return false;
        }
        LOG_DEBUG("Database", "messages表创建成功");

        // ========== 插入默认管理员 ==========
        query.prepare("SELECT COUNT(*) FROM admins WHERE username = ?");
        query.addBindValue("admin");
        if (query.exec() && query.next() && query.value(0).toInt() == 0) {
            QString salt = generateSalt();
            QString hash = hashPassword("Admin@123", salt);
            query.prepare("INSERT INTO admins (username, password, salt) VALUES (?, ?, ?)");
            query.addBindValue("admin");
            query.addBindValue(hash);
            query.addBindValue(salt);
            if (query.exec()) {
                LOG_INFO("Database", "默认管理员账号已创建 (admin/Admin@123)");
            }
        }

        return true;
        
    } catch (const std::exception &e) {
        m_lastError = QString("创建表异常: %1").arg(e.what());
        LOG_ERROR("Database", m_lastError);
        return false;
    }
}

/**
 * @brief 验证管理员登录
 * 
 * 通过用户名查询管理员信息，然后使用相同的盐值对输入密码进行哈希，
 * 比较哈希值是否匹配来验证密码正确性。
 * 
 * @param username 管理员用户名
 * @param password 输入的密码（明文）
 * @return DbResult 验证结果
 */
DbResult Database::validateAdmin(const QString &username, const QString &password) {
    DbResult result{false, ""};
    
    LOG_DEBUG("Database", QString("验证管理员登录: %1").arg(username));
    
    try {
        QSqlQuery query(m_db);
        query.prepare("SELECT password, salt FROM admins WHERE username = ?");
        query.addBindValue(username);
        
        if (!query.exec()) {
            QString errorMsg = QString("查询失败: %1").arg(query.lastError().text());
            LOG_ERROR("Database", errorMsg);
            result.errorMessage = errorMsg;
            m_lastError = errorMsg;
            emit errorOccurred(errorMsg);
            return result;
        }
        
        if (query.next()) {
            QString storedHash = query.value(0).toString();
            QString salt = query.value(1).toString();
            QString inputHash = hashPassword(password, salt);
            
            if (storedHash == inputHash) {
                result.success = true;
                LOG_INFO("Database", QString("管理员 %1 验证成功").arg(username));
            } else {
                result.errorMessage = "密码错误";
                LOG_WARNING("Database", QString("管理员 %1 密码错误").arg(username));
            }
        } else {
            result.errorMessage = "管理员账号不存在";
            LOG_WARNING("Database", QString("管理员账号不存在: %1").arg(username));
        }
        
    } catch (const std::exception &e) {
        QString errorMsg = QString("验证管理员异常: %1").arg(e.what());
        LOG_ERROR("Database", errorMsg);
        result.errorMessage = errorMsg;
        m_lastError = errorMsg;
        emit errorOccurred(errorMsg);
    }
    
    return result;
}

/**
 * @brief 注册新用户
 * 
 * 注册流程：
 * 1. 验证密码复杂度
 * 2. 检查用户名是否已存在
 * 3. 生成盐值并哈希密码
 * 4. 插入用户记录
 * 
 * @param username 用户名
 * @param password 密码（明文）
 * @param nickname 昵称
 * @return DbResult 注册结果
 */
DbResult Database::registerUser(const QString &username, const QString &password, const QString &nickname) {
    DbResult result{false, ""};
    
    LOG_DEBUG("Database", QString("注册新用户: %1").arg(username));
    
    try {
        // 步骤1: 验证密码复杂度
        auto pwdCheck = validatePasswordComplexity(password);
        if (!pwdCheck.valid) {
            result.errorMessage = pwdCheck.errorMessage;
            LOG_WARNING("Database", QString("用户 %1 注册失败: %2").arg(username).arg(pwdCheck.errorMessage));
            return result;
        }
        
        // 步骤2: 检查用户名是否已存在
        QSqlQuery checkQuery(m_db);
        checkQuery.prepare("SELECT COUNT(*) FROM users WHERE username = ?");
        checkQuery.addBindValue(username);
        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            result.errorMessage = "用户名已存在";
            LOG_WARNING("Database", QString("用户名已存在: %1").arg(username));
            return result;
        }
        
        // 步骤3: 生成盐值并哈希密码
        QString salt = generateSalt();
        QString hash = hashPassword(password, salt);
        
        // 步骤4: 插入用户记录
        QSqlQuery query(m_db);
        query.prepare("INSERT INTO users (username, password, salt, nickname, role) VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(username);
        query.addBindValue(hash);
        query.addBindValue(salt);
        query.addBindValue(nickname);
        query.addBindValue(static_cast<int>(UserRole::User));
        
        if (!query.exec()) {
            QString errorMsg = QString("注册失败: %1").arg(query.lastError().text());
            LOG_ERROR("Database", errorMsg);
            result.errorMessage = errorMsg;
            m_lastError = errorMsg;
            emit errorOccurred(errorMsg);
            return result;
        }
        
        result.success = true;
        LOG_INFO("Database", QString("用户 %1 注册成功").arg(username));
        
    } catch (const std::exception &e) {
        QString errorMsg = QString("注册用户异常: %1").arg(e.what());
        LOG_ERROR("Database", errorMsg);
        result.errorMessage = errorMsg;
        m_lastError = errorMsg;
        emit errorOccurred(errorMsg);
    }
    
    return result;
}

/**
 * @brief 验证用户登录
 * 
 * 与管理员验证类似，通过盐值哈希比对验证密码。
 * 
 * @param username 用户名
 * @param password 密码（明文）
 * @return DbResult 验证结果
 */
DbResult Database::validateUser(const QString &username, const QString &password) {
    DbResult result{false, ""};
    
    LOG_DEBUG("Database", QString("验证用户登录: %1").arg(username));
    
    try {
        QSqlQuery query(m_db);
        query.prepare("SELECT password, salt FROM users WHERE username = ?");
        query.addBindValue(username);
        
        if (!query.exec()) {
            QString errorMsg = QString("查询失败: %1").arg(query.lastError().text());
            LOG_ERROR("Database", errorMsg);
            result.errorMessage = errorMsg;
            m_lastError = errorMsg;
            emit errorOccurred(errorMsg);
            return result;
        }
        
        if (query.next()) {
            QString storedHash = query.value(0).toString();
            QString salt = query.value(1).toString();
            QString inputHash = hashPassword(password, salt);
            
            if (storedHash == inputHash) {
                result.success = true;
                LOG_INFO("Database", QString("用户 %1 登录成功").arg(username));
            } else {
                result.errorMessage = "密码错误";
                LOG_WARNING("Database", QString("用户 %1 密码错误").arg(username));
            }
        } else {
            result.errorMessage = "用户不存在";
            LOG_WARNING("Database", QString("用户不存在: %1").arg(username));
        }
        
    } catch (const std::exception &e) {
        QString errorMsg = QString("验证用户异常: %1").arg(e.what());
        LOG_ERROR("Database", errorMsg);
        result.errorMessage = errorMsg;
        m_lastError = errorMsg;
        emit errorOccurred(errorMsg);
    }
    
    return result;
}

/**
 * @brief 获取用户详细信息
 * 
 * @param username 用户名
 * @return UserInfo 用户信息（用户不存在时返回空结构体）
 */
UserInfo Database::getUserInfo(const QString &username) {
    UserInfo info;
    
    try {
        QSqlQuery query(m_db);
        query.prepare("SELECT id, username, nickname, role, register_time FROM users WHERE username = ?");
        query.addBindValue(username);
        
        if (query.exec() && query.next()) {
            info.id = query.value(0).toInt();
            info.username = query.value(1).toString();
            info.nickname = query.value(2).toString();
            info.role = static_cast<UserRole>(query.value(3).toInt());
            info.registerTime = query.value(4).toString();
            LOG_DEBUG("Database", QString("获取用户信息成功: %1").arg(username));
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Database", QString("获取用户信息异常: %1").arg(e.what()));
    }
    
    return info;
}

/**
 * @brief 获取所有用户列表
 * 
 * @return QList<UserInfo> 用户信息列表
 */
QList<UserInfo> Database::getAllUsers() {
    QList<UserInfo> users;
    
    try {
        QSqlQuery query(m_db);
        query.exec("SELECT id, username, nickname, role, register_time FROM users");
        
        while (query.next()) {
            UserInfo info;
            info.id = query.value(0).toInt();
            info.username = query.value(1).toString();
            info.nickname = query.value(2).toString();
            info.role = static_cast<UserRole>(query.value(3).toInt());
            info.registerTime = query.value(4).toString();
            users.append(info);
        }
        LOG_DEBUG("Database", QString("获取用户列表成功，共 %1 个用户").arg(users.size()));
    } catch (const std::exception &e) {
        LOG_ERROR("Database", QString("获取用户列表异常: %1").arg(e.what()));
    }
    
    return users;
}

/**
 * @brief 设置用户角色
 * 
 * @param username 用户名
 * @param role 新角色
 * @return DbResult 操作结果
 */
DbResult Database::setUserRole(const QString &username, UserRole role) {
    DbResult result{false, ""};
    
    LOG_DEBUG("Database", QString("设置用户角色: %1 -> %2").arg(username).arg(static_cast<int>(role)));
    
    try {
        QSqlQuery query(m_db);
        query.prepare("UPDATE users SET role = ? WHERE username = ?");
        query.addBindValue(static_cast<int>(role));
        query.addBindValue(username);
        
        if (!query.exec()) {
            QString errorMsg = QString("设置用户角色失败: %1").arg(query.lastError().text());
            LOG_ERROR("Database", errorMsg);
            result.errorMessage = errorMsg;
            m_lastError = errorMsg;
            emit errorOccurred(errorMsg);
            return result;
        }
        
        if (query.numRowsAffected() == 0) {
            result.errorMessage = "用户不存在";
            LOG_WARNING("Database", QString("设置角色失败，用户不存在: %1").arg(username));
            return result;
        }
        
        result.success = true;
        LOG_INFO("Database", QString("用户 %1 角色已更新为 %2").arg(username).arg(static_cast<int>(role)));
        
    } catch (const std::exception &e) {
        QString errorMsg = QString("设置用户角色异常: %1").arg(e.what());
        LOG_ERROR("Database", errorMsg);
        result.errorMessage = errorMsg;
        m_lastError = errorMsg;
        emit errorOccurred(errorMsg);
    }
    
    return result;
}

/**
 * @brief 获取用户角色
 * 
 * @param username 用户名
 * @return UserRole 用户角色（用户不存在时返回 User）
 */
UserRole Database::getUserRole(const QString &username) {
    try {
        QSqlQuery query(m_db);
        query.prepare("SELECT role FROM users WHERE username = ?");
        query.addBindValue(username);
        
        if (query.exec() && query.next()) {
            return static_cast<UserRole>(query.value(0).toInt());
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Database", QString("获取用户角色异常: %1").arg(e.what()));
    }
    
    return UserRole::User;
}

/**
 * @brief 保存聊天消息到数据库
 * 
 * @param from 发送者用户名
 * @param to 接收者用户名（群聊时为空）
 * @param message 消息内容
 * @param isGroup 是否为群聊消息
 * @return DbResult 保存结果
 */
DbResult Database::saveMessage(const QString &from, const QString &to, const QString &message, bool isGroup) {
    DbResult result{false, ""};
    
    try {
        QSqlQuery query(m_db);
        query.prepare("INSERT INTO messages (from_user, to_user, message, is_group) VALUES (?, ?, ?, ?)");
        query.addBindValue(from);
        query.addBindValue(isGroup ? "" : to);
        query.addBindValue(message);
        query.addBindValue(isGroup ? 1 : 0);
        
        if (!query.exec()) {
            QString errorMsg = QString("保存消息失败: %1").arg(query.lastError().text());
            LOG_ERROR("Database", errorMsg);
            result.errorMessage = errorMsg;
            m_lastError = errorMsg;
            emit errorOccurred(errorMsg);
            return result;
        }
        
        result.success = true;
        LOG_DEBUG("Database", QString("消息已保存: %1 -> %2").arg(from).arg(isGroup ? "群聊" : to));
        
    } catch (const std::exception &e) {
        QString errorMsg = QString("保存消息异常: %1").arg(e.what());
        LOG_ERROR("Database", errorMsg);
        result.errorMessage = errorMsg;
        m_lastError = errorMsg;
        emit errorOccurred(errorMsg);
    }
    
    return result;
}

/**
 * @brief 获取两个用户之间的私聊消息历史
 * 
 * 查询两个用户之间的所有私聊消息，按时间正序返回。
 * 
 * @param user1 用户1的用户名
 * @param user2 用户2的用户名
 * @param limit 返回的最大消息数量
 * @return QList<MessageRecord> 消息记录列表
 */
QList<MessageRecord> Database::getMessageHistory(const QString &user1, const QString &user2, int limit) {
    QList<MessageRecord> records;
    
    try {
        QSqlQuery query(m_db);
        query.prepare(R"(
            SELECT id, from_user, to_user, message, timestamp, is_group 
            FROM messages 
            WHERE is_group = 0 AND ((from_user = ? AND to_user = ?) OR (from_user = ? AND to_user = ?))
            ORDER BY timestamp DESC LIMIT ?
        )");
        query.addBindValue(user1);
        query.addBindValue(user2);
        query.addBindValue(user2);
        query.addBindValue(user1);
        query.addBindValue(limit);
        
        if (query.exec()) {
            while (query.next()) {
                MessageRecord record;
                record.id = query.value(0).toInt();
                record.fromUser = query.value(1).toString();
                record.toUser = query.value(2).toString();
                record.message = query.value(3).toString();
                record.timestamp = query.value(4).toString();
                record.isGroupMessage = query.value(5).toBool();
                records.prepend(record);  // 反转顺序，最早的在前
            }
            LOG_DEBUG("Database", QString("获取私聊历史: %1 <-> %2, 共 %3 条")
                      .arg(user1).arg(user2).arg(records.size()));
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Database", QString("获取私聊历史异常: %1").arg(e.what()));
    }
    
    return records;
}

/**
 * @brief 获取群聊消息历史
 * 
 * 查询所有群聊消息，按时间正序返回。
 * 
 * @param limit 返回的最大消息数量
 * @return QList<MessageRecord> 消息记录列表
 */
QList<MessageRecord> Database::getGroupMessageHistory(int limit) {
    QList<MessageRecord> records;
    
    try {
        QSqlQuery query(m_db);
        query.prepare(R"(
            SELECT id, from_user, to_user, message, timestamp, is_group 
            FROM messages 
            WHERE is_group = 1
            ORDER BY timestamp DESC LIMIT ?
        )");
        query.addBindValue(limit);
        
        if (query.exec()) {
            while (query.next()) {
                MessageRecord record;
                record.id = query.value(0).toInt();
                record.fromUser = query.value(1).toString();
                record.toUser = query.value(2).toString();
                record.message = query.value(3).toString();
                record.timestamp = query.value(4).toString();
                record.isGroupMessage = query.value(5).toBool();
                records.prepend(record);  // 反转顺序，最早的在前
            }
            LOG_DEBUG("Database", QString("获取群聊历史: 共 %1 条").arg(records.size()));
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Database", QString("获取群聊历史异常: %1").arg(e.what()));
    }
    
    return records;
}
