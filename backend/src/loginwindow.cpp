/**
 * @file loginwindow.cpp
 * @brief 登录窗口类实现文件
 */

#include "loginwindow.h"
#include "logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

LoginWindow::LoginWindow(Database *db, QWidget *parent)
    : QWidget(parent), m_db(db) {
    
    setWindowTitle("管理员登录 - Chat Server");
    setFixedSize(350, 200);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 标题
    QLabel *titleLabel = new QLabel("聊天室服务器管理系统");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; margin: 10px;");
    mainLayout->addWidget(titleLabel);
    
    // 表单布局
    QFormLayout *formLayout = new QFormLayout();
    
    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText("请输入管理员用户名");
    
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setPlaceholderText("请输入密码");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    
    formLayout->addRow("用户名:", m_usernameEdit);
    formLayout->addRow("密  码:", m_passwordEdit);
    mainLayout->addLayout(formLayout);
    
    // 状态标签（显示错误信息）
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: red;");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);
    
    // 登录按钮
    QPushButton *loginBtn = new QPushButton("登录");
    loginBtn->setStyleSheet("padding: 8px;");
    connect(loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    mainLayout->addWidget(loginBtn);
    
    mainLayout->addStretch();
    
    LOG_DEBUG("LoginWindow", "登录窗口初始化完成");
}

/**
 * @brief 处理登录按钮点击
 * 
 * 验证流程：
 * 1. 检查输入是否为空
 * 2. 调用数据库验证管理员身份
 * 3. 验证成功发出信号，失败显示错误信息
 */
void LoginWindow::onLoginClicked() {
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    
    // 检查输入
    if (username.isEmpty() || password.isEmpty()) {
        m_statusLabel->setText("请输入用户名和密码");
        LOG_WARNING("LoginWindow", "登录尝试: 用户名或密码为空");
        return;
    }
    
    LOG_DEBUG("LoginWindow", QString("尝试登录: %1").arg(username));
    
    // 验证管理员身份
    DbResult result = m_db->validateAdmin(username, password);
    if (result.success) {
        LOG_INFO("LoginWindow", QString("管理员 %1 登录成功").arg(username));
        emit loginSuccess();
    } else {
        m_statusLabel->setText(result.errorMessage);
        m_passwordEdit->clear();
        LOG_WARNING("LoginWindow", QString("登录失败: %1 (用户: %2)").arg(result.errorMessage).arg(username));
    }
}
