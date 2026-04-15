/**
 * @file database.h
 * @brief 数据库操作类头文件
 * 
 * 提供数据库连接、用户管理、消息存储等核心功能。
 * 支持 SQLite、MySQL、PostgreSQL 三种数据库类型。
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QList>
#include <stdexcept>

/**
 * @enum UserRole
 * @brief 用户角色枚举
 * 
 * 定义系统中的用户权限级别：
 * - User: 普通用户，只能进行基本聊天操作
 * - Moderator: 版主，可以管理聊天内容
 * - Admin: 管理员，拥有所有权限
 */
enum class UserRole {
    User = 0,       ///< 普通用户
    Moderator = 1,  ///< 版主
    Admin = 2       ///< 管理员
};

/**
 * @struct UserInfo
 * @brief 用户信息结构体
 * 
 * 存储用户的完整信息，用于用户查询和显示。
 */
struct UserInfo {
    int id;              ///< 用户ID（数据库主键）
    QString username;    ///< 用户名（唯一标识）
    QString password;    ///< 密码哈希值（不存储明文）
    QString salt;        ///< 密码盐值（用于增强安全性）
    QString nickname;    ///< 用户昵称（显示名称）
    QString registerTime;///< 注册时间
    UserRole role;       ///< 用户角色
};

/**
 * @struct MessageRecord
 * @brief 消息记录结构体
 * 
 * 存储聊天消息的完整信息，支持私聊和群聊。
 */
struct MessageRecord {
    int id;              ///< 消息ID
    QString fromUser;    ///< 发送者用户名
    QString toUser;      ///< 接收者用户名（群聊时为空）
    QString message;     ///< 消息内容
    QString timestamp;   ///< 发送时间
    bool isGroupMessage; ///< 是否为群聊消息
};

/**
 * @struct DbResult
 * @brief 数据库操作结果结构体
 * 
 * 用于返回数据库操作的执行结果，包含成功标志和错误信息。
 */
struct DbResult {
    bool success;           ///< 操作是否成功
    QString errorMessage;   ///< 错误信息（失败时有效）
};

/**
 * @struct PasswordValidation
 * @brief 密码验证结果结构体
 * 
 * 用于返回密码复杂度验证的结果。
 */
struct PasswordValidation {
    bool valid;             ///< 密码是否符合要求
    QString errorMessage;   ///< 不符合要求时的错误提示
};

/**
 * @class DatabaseException
 * @brief 数据库异常类
 * 
 * 用于数据库操作中的异常处理，继承自 std::runtime_error。
 * 提供详细的错误信息和错误代码。
 */
class DatabaseException : public std::runtime_error {
public:
    /**
     * @brief 构造函数
     * @param message 错误消息
     * @param code 错误代码（可选）
     */
    explicit DatabaseException(const QString &message, int code = 0)
        : std::runtime_error(message.toStdString())
        , m_message(message)
        , m_code(code) {}

    /**
     * @brief 获取错误消息
     * @return QString 错误消息
     */
    QString message() const { return m_message; }

    /**
     * @brief 获取错误代码
     * @return int 错误代码
     */
    int code() const { return m_code; }

private:
    QString m_message;  ///< 错误消息
    int m_code;         ///< 错误代码
};

/**
 * @class Database
 * @brief 数据库操作类
 * 
 * 提供完整的数据库操作功能，包括：
 * - 数据库连接和初始化
 * - 管理员验证
 * - 用户注册、登录验证
 * - 用户信息查询和角色管理
 * - 消息存储和历史记录查询
 * 
 * 安全特性：
 * - 使用盐值+多轮哈希存储密码
 * - 密码复杂度验证
 * - 参数化查询防止SQL注入
 * 
 * @note 支持 SQLite、MySQL、PostgreSQL 数据库
 * 
 * 使用示例：
 * @code
 * Database db;
 * DbResult result = db.init("sqlite", "", 0, "chat.db", "", "");
 * if (result.success) {
 *     DbResult loginResult = db.validateUser("username", "password");
 *     if (loginResult.success) {
 *         // 登录成功
 *     }
 * }
 * @endcode
 */
class Database : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit Database(QObject *parent = nullptr);

    /**
     * @brief 析构函数，关闭数据库连接
     */
    ~Database();

    /**
     * @brief 初始化数据库连接
     * @param type 数据库类型（"sqlite"、"mysql"、"postgresql"）
     * @param host 数据库主机地址（SQLite时忽略）
     * @param port 数据库端口号（SQLite时忽略）
     * @param dbName 数据库名称或SQLite文件路径
     * @param user 数据库用户名（SQLite时忽略）
     * @param pass 数据库密码（SQLite时忽略）
     * @return DbResult 初始化结果
     * 
     * @throws DatabaseException 当数据库驱动不可用或连接失败时
     * 
     * @note 初始化时会自动创建所需的数据表
     */
    DbResult init(const QString &type, const QString &host, int port,
                  const QString &dbName, const QString &user, const QString &pass);

    /**
     * @brief 验证管理员登录
     * @param username 管理员用户名
     * @param password 管理员密码（明文）
     * @return DbResult 验证结果
     */
    DbResult validateAdmin(const QString &username, const QString &password);

    /**
     * @brief 注册新用户
     * @param username 用户名（唯一）
     * @param password 密码（明文，会进行复杂度验证）
     * @param nickname 用户昵称
     * @return DbResult 注册结果
     * 
     * 密码要求：
     * - 长度至少8位
     * - 包含大写字母
     * - 包含小写字母
     * - 包含数字
     * - 包含特殊字符
     */
    DbResult registerUser(const QString &username, const QString &password, const QString &nickname);

    /**
     * @brief 验证用户登录
     * @param username 用户名
     * @param password 密码（明文）
     * @return DbResult 验证结果
     */
    DbResult validateUser(const QString &username, const QString &password);

    /**
     * @brief 获取用户信息
     * @param username 用户名
     * @return UserInfo 用户信息结构体（用户不存在时返回空结构体）
     */
    UserInfo getUserInfo(const QString &username);

    /**
     * @brief 获取所有用户列表
     * @return QList<UserInfo> 用户信息列表
     */
    QList<UserInfo> getAllUsers();
    
    /**
     * @brief 验证密码复杂度
     * @param password 待验证的密码
     * @return PasswordValidation 验证结果
     * 
     * @note 静态方法，可不创建实例直接调用
     */
    static PasswordValidation validatePasswordComplexity(const QString &password);
    
    /**
     * @brief 设置用户角色
     * @param username 用户名
     * @param role 新角色
     * @return DbResult 操作结果
     */
    DbResult setUserRole(const QString &username, UserRole role);

    /**
     * @brief 获取用户角色
     * @param username 用户名
     * @return UserRole 用户角色（用户不存在时返回 User）
     */
    UserRole getUserRole(const QString &username);
    
    /**
     * @brief 保存聊天消息
     * @param from 发送者用户名
     * @param to 接收者用户名（群聊时为空或"all"）
     * @param message 消息内容
     * @param isGroup 是否为群聊消息
     * @return DbResult 保存结果
     */
    DbResult saveMessage(const QString &from, const QString &to, const QString &message, bool isGroup);

    /**
     * @brief 获取私聊消息历史
     * @param user1 用户1的用户名
     * @param user2 用户2的用户名
     * @param limit 返回的最大消息数量（默认100）
     * @return QList<MessageRecord> 消息记录列表（按时间正序）
     */
    QList<MessageRecord> getMessageHistory(const QString &user1, const QString &user2, int limit = 100);

    /**
     * @brief 获取群聊消息历史
     * @param limit 返回的最大消息数量（默认100）
     * @return QList<MessageRecord> 消息记录列表（按时间正序）
     */
    QList<MessageRecord> getGroupMessageHistory(int limit = 100);
    
    /**
     * @brief 获取最后一次错误信息
     * @return QString 错误信息
     */
    QString lastError() const { return m_lastError; }

signals:
    /**
     * @brief 错误发生信号
     * @param error 错误信息
     * 
     * 当数据库操作发生错误时发出此信号。
     */
    void errorOccurred(const QString &error);

private:
    QSqlDatabase m_db;      ///< 数据库连接对象
    QString m_lastError;    ///< 最后一次错误信息

    /**
     * @brief 创建数据库表
     * @return bool 是否成功创建所有表
     * 
     * 创建以下表：
     * - admins: 管理员表
     * - users: 用户表
     * - messages: 消息历史表
     */
    bool createTables();

    /**
     * @brief 生成随机盐值
     * @return QString 16字节的十六进制盐值字符串
     */
    QString generateSalt();

    /**
     * @brief 对密码进行哈希处理
     * @param password 明文密码
     * @param salt 盐值
     * @return QString 哈希后的密码（十六进制字符串）
     * 
     * 使用 SHA-256 算法进行 10000 轮迭代哈希。
     */
    QString hashPassword(const QString &password, const QString &salt);
};

#endif // DATABASE_H
