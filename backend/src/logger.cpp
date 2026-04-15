/**
 * @file logger.cpp
 * @brief 日志系统实现文件
 */

#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// 初始化静态单例指针
Logger* Logger::s_instance = nullptr;

Logger* Logger::instance() {
    if (!s_instance) {
        s_instance = new Logger();
    }
    return s_instance;
}

Logger::Logger(QObject *parent) : QObject(parent) {}

Logger::~Logger() {
    QMutexLocker locker(&m_mutex);
    if (m_logStream) {
        m_logStream->flush();
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }
}

void Logger::setLogFile(const QString &filePath) {
    QMutexLocker locker(&m_mutex);
    
    // 清理旧的文件资源
    if (m_logStream) {
        m_logStream->flush();
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }
    
    // 确保目录存在
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 打开新的日志文件
    m_logFile = new QFile(filePath);
    if (m_logFile->open(QIODevice::Append | QIODevice::Text)) {
        m_logStream = new QTextStream(m_logFile);
        m_logStream->setEncoding(QStringConverter::Utf8);
    } else {
        qWarning() << "Logger: 无法打开日志文件:" << filePath;
        delete m_logFile;
        m_logFile = nullptr;
    }
}

void Logger::enablePersistence(bool enable) {
    m_persistence = enable;
}

void Logger::setMinLevel(LogLevel level) {
    m_minLevel = level;
}

QString Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

void Logger::debug(const QString &category, const QString &message) {
    log(LogLevel::DEBUG, category, message);
}

void Logger::info(const QString &category, const QString &message) {
    log(LogLevel::INFO, category, message);
}

void Logger::warning(const QString &category, const QString &message) {
    log(LogLevel::WARNING, category, message);
}

void Logger::error(const QString &category, const QString &message) {
    log(LogLevel::ERROR, category, message);
}

void Logger::log(LogLevel level, const QString &category, const QString &message) {
    // 检查日志级别过滤
    if (level < m_minLevel) {
        return;
    }
    
    // 格式化日志消息: [时间] [级别] [分类] 消息
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = levelToString(level);
    QString formattedMsg = QString("[%1] [%2] [%3] %4")
                           .arg(timestamp)
                           .arg(levelStr, -7)  // 左对齐，宽度7
                           .arg(category)
                           .arg(message);
    
    // 发出信号供UI显示
    emit logWritten(level, formattedMsg);
    
    // 写入文件
    if (m_persistence) {
        writeToFile(formattedMsg);
    }
}

void Logger::writeToFile(const QString &message) {
    QMutexLocker locker(&m_mutex);
    if (m_logStream) {
        *m_logStream << message << "\n";
        m_logStream->flush();
    }
}
