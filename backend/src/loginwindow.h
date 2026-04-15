/**
 * @file loginwindow.h
 * @brief 登录窗口类头文件
 * 
 * 提供管理员登录界面，验证管理员身份后进入主界面。
 */

#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "database.h"

/**
 * @class LoginWindow
 * @brief 管理员登录窗口
 * 
 * 提供简洁的登录界面，包括：
 * - 用户名输入框
 * - 密码输入框
 * - 登录按钮
 * - 状态提示标签
 * 
 * 登录成功后发出 loginSuccess 信号。
 */
class LoginWindow : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param db 数据库对象指针
     * @param parent 父窗口指针
     */
    explicit LoginWindow(Database *db, QWidget *parent = nullptr);

signals:
    /**
     * @brief 登录成功信号
     * 
     * 当管理员验证通过后发出此信号。
     */
    void loginSuccess();

private slots:
    /**
     * @brief 登录按钮点击处理
     * 
     * 验证用户名和密码，成功则发出 loginSuccess 信号。
     */
    void onLoginClicked();

private:
    Database *m_db;              ///< 数据库对象指针
    QLineEdit *m_usernameEdit;   ///< 用户名输入框
    QLineEdit *m_passwordEdit;   ///< 密码输入框
    QLabel *m_statusLabel;       ///< 状态提示标签
};

#endif // LOGINWINDOW_H
