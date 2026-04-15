/**
 * @file logger.h
 * @brief 日志系统头文件
 * 
 * 提供统一的日志记录功能，支持多种日志级别（DEBUG、INFO、WARNING、ERROR），
 * 支持日志持久化到文件，线程安全。
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>

/**
 * @enum LogLevel
 * @brief 日志级别枚举
 * 
 * 定义四种日志级别，用于区分日志的重要程度：
 * - DEBUG: 调试信息，用于开发阶段的详细跟踪
 * - INFO: 一般信息，记录正常的操作流程
 * - WARNING: 警告信息，表示潜在问题但不影响运行
 * - ERROR: 错误信息，表示发生了需要处理的错误
 */
enum class LogLevel {
    DEBUG = 0,    ///< 调试级别
    INFO = 1,     ///< 信息级别
    WARNING = 2,  ///< 警告级别
    ERROR = 3     ///< 错误级别
};

/**
 * @class Logger
 * @brief 日志管理类
 * 
 * 单例模式实现的日志管理器，提供以下功能：
 * - 多级别日志记录（DEBUG/INFO/WARNING/ERROR）
 * - 日志文件持久化
 * - 线程安全的日志写入
 * - 可配置的最小日志级别过滤
 * 
 * @note 使用单例模式，通过 Logger::instance() 获取实例
 * 
 * 使用示例：
 * @code
 * Logger::instance()->setLogFile("/path/to/log.txt");
 * Logger::instance()->setMinLevel(LogLevel::INFO);
 * Logger::instance()->info("Server", "服务器启动成功");
 * Logger::instance()->error("Database", "连接失败: %s", errorMsg);
 * @endcode
 */
class Logger : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取Logger单例实例
     * @return Logger* 单例指针
     */
    static Logger* instance();

    /**
     * @brief 设置日志文件路径
     * @param filePath 日志文件的完整路径
     * 
     * 如果文件不存在会自动创建，如果目录不存在也会自动创建。
     * 日志以追加模式写入文件。
     */
    void setLogFile(const QString &filePath);

    /**
     * @brief 启用或禁用日志持久化
     * @param enable true启用持久化，false禁用
     */
    void enablePersistence(bool enable);

    /**
     * @brief 设置最小日志级别
     * @param level 最小日志级别，低于此级别的日志将被忽略
     * 
     * 例如设置为 WARNING，则 DEBUG 和 INFO 级别的日志不会被记录。
     */
    void setMinLevel(LogLevel level);

    /**
     * @brief 获取当前最小日志级别
     * @return LogLevel 当前设置的最小日志级别
     */
    LogLevel minLevel() const { return m_minLevel; }

    /**
     * @brief 记录DEBUG级别日志
     * @param category 日志分类（如 "Server"、"Database"）
     * @param message 日志消息内容
     */
    void debug(const QString &category, const QString &message);

    /**
     * @brief 记录INFO级别日志
     * @param category 日志分类
     * @param message 日志消息内容
     */
    void info(const QString &category, const QString &message);

    /**
     * @brief 记录WARNING级别日志
     * @param category 日志分类
     * @param message 日志消息内容
     */
    void warning(const QString &category, const QString &message);

    /**
     * @brief 记录ERROR级别日志
     * @param category 日志分类
     * @param message 日志消息内容
     */
    void error(const QString &category, const QString &message);

    /**
     * @brief 通用日志记录方法
     * @param level 日志级别
     * @param category 日志分类
     * @param message 日志消息内容
     */
    void log(LogLevel level, const QString &category, const QString &message);

signals:
    /**
     * @brief 日志消息信号
     * @param level 日志级别
     * @param formattedMessage 格式化后的完整日志消息
     * 
     * 当有新日志记录时发出此信号，可用于UI显示。
     */
    void logWritten(LogLevel level, const QString &formattedMessage);

private:
    /**
     * @brief 私有构造函数（单例模式）
     * @param parent 父对象指针
     */
    explicit Logger(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数，清理文件资源
     */
    ~Logger();

    /**
     * @brief 将日志级别转换为字符串
     * @param level 日志级别
     * @return QString 级别对应的字符串表示
     */
    QString levelToString(LogLevel level) const;

    /**
     * @brief 将日志写入文件
     * @param message 要写入的日志消息
     */
    void writeToFile(const QString &message);

    static Logger *s_instance;     ///< 单例实例指针
    QFile *m_logFile = nullptr;    ///< 日志文件对象
    QTextStream *m_logStream = nullptr;  ///< 日志文件流
    bool m_persistence = false;    ///< 是否启用持久化
    LogLevel m_minLevel = LogLevel::DEBUG;  ///< 最小日志级别
    QMutex m_mutex;                ///< 线程安全互斥锁
};

// 便捷宏定义，简化日志调用
#define LOG_DEBUG(category, message) Logger::instance()->debug(category, message)
#define LOG_INFO(category, message) Logger::instance()->info(category, message)
#define LOG_WARNING(category, message) Logger::instance()->warning(category, message)
#define LOG_ERROR(category, message) Logger::instance()->error(category, message)

#endif // LOGGER_H
