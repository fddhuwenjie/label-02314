#include <QtTest/QtTest>
#include "../src/database.h"

class TestDatabase : public QObject {
    Q_OBJECT

private:
    Database *db;

private slots:
    void initTestCase() {
        db = new Database();
        DbResult result = db->init("sqlite", "", 0, ":memory:", "", "");
        QVERIFY2(result.success, qPrintable(result.errorMessage));
    }

    void cleanupTestCase() {
        delete db;
    }

    // 密码复杂度验证测试
    void testPasswordComplexity_TooShort() {
        auto result = Database::validatePasswordComplexity("Abc1!");
        QVERIFY(!result.valid);
        QCOMPARE(result.errorMessage, QString("密码长度至少8位"));
    }

    void testPasswordComplexity_NoUppercase() {
        auto result = Database::validatePasswordComplexity("abcd1234!");
        QVERIFY(!result.valid);
        QCOMPARE(result.errorMessage, QString("密码需包含大写字母"));
    }

    void testPasswordComplexity_NoLowercase() {
        auto result = Database::validatePasswordComplexity("ABCD1234!");
        QVERIFY(!result.valid);
        QCOMPARE(result.errorMessage, QString("密码需包含小写字母"));
    }

    void testPasswordComplexity_NoDigit() {
        auto result = Database::validatePasswordComplexity("Abcdefgh!");
        QVERIFY(!result.valid);
        QCOMPARE(result.errorMessage, QString("密码需包含数字"));
    }

    void testPasswordComplexity_NoSpecial() {
        auto result = Database::validatePasswordComplexity("Abcdefg1");
        QVERIFY(!result.valid);
        QCOMPARE(result.errorMessage, QString("密码需包含特殊字符"));
    }

    void testPasswordComplexity_Valid() {
        auto result = Database::validatePasswordComplexity("Abcdefg1!");
        QVERIFY(result.valid);
        QVERIFY(result.errorMessage.isEmpty());
    }

    // 用户注册测试
    void testRegisterUser_Success() {
        DbResult result = db->registerUser("testuser", "Password1!", "测试用户");
        QVERIFY2(result.success, qPrintable(result.errorMessage));
    }

    void testRegisterUser_DuplicateUsername() {
        db->registerUser("duplicate", "Password1!", "重复用户");
        DbResult result = db->registerUser("duplicate", "Password2!", "重复用户2");
        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QString("用户名已存在"));
    }

    void testRegisterUser_WeakPassword() {
        DbResult result = db->registerUser("weakpwd", "123", "弱密码用户");
        QVERIFY(!result.success);
    }

    // 用户验证测试
    void testValidateUser_Success() {
        db->registerUser("logintest", "Password1!", "登录测试");
        DbResult result = db->validateUser("logintest", "Password1!");
        QVERIFY2(result.success, qPrintable(result.errorMessage));
    }

    void testValidateUser_WrongPassword() {
        db->registerUser("wrongpwd", "Password1!", "错误密码测试");
        DbResult result = db->validateUser("wrongpwd", "WrongPass1!");
        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QString("密码错误"));
    }

    void testValidateUser_NonExistent() {
        DbResult result = db->validateUser("nonexistent", "Password1!");
        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QString("用户不存在"));
    }

    // 用户信息测试
    void testGetUserInfo() {
        db->registerUser("infotest", "Password1!", "信息测试");
        UserInfo info = db->getUserInfo("infotest");
        QCOMPARE(info.username, QString("infotest"));
        QCOMPARE(info.nickname, QString("信息测试"));
        QCOMPARE(info.role, UserRole::User);
    }

    // 用户角色测试
    void testSetUserRole() {
        db->registerUser("roletest", "Password1!", "角色测试");
        DbResult result = db->setUserRole("roletest", UserRole::Admin);
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        QCOMPARE(db->getUserRole("roletest"), UserRole::Admin);
    }

    void testSetUserRole_NonExistent() {
        DbResult result = db->setUserRole("nonexistent_role", UserRole::Admin);
        QVERIFY(!result.success);
    }

    // 消息存储测试
    void testSaveMessage_Private() {
        db->registerUser("sender", "Password1!", "发送者");
        db->registerUser("receiver", "Password1!", "接收者");
        DbResult result = db->saveMessage("sender", "receiver", "Hello!", false);
        QVERIFY2(result.success, qPrintable(result.errorMessage));
    }

    void testSaveMessage_Group() {
        DbResult result = db->saveMessage("sender", "", "群发消息", true);
        QVERIFY2(result.success, qPrintable(result.errorMessage));
    }

    void testGetMessageHistory() {
        db->registerUser("hist1", "Password1!", "历史1");
        db->registerUser("hist2", "Password1!", "历史2");
        db->saveMessage("hist1", "hist2", "消息1", false);
        db->saveMessage("hist2", "hist1", "消息2", false);
        
        QList<MessageRecord> records = db->getMessageHistory("hist1", "hist2", 10);
        QVERIFY(records.size() >= 2);
    }

    void testGetGroupMessageHistory() {
        db->saveMessage("groupsender", "", "群发测试1", true);
        db->saveMessage("groupsender", "", "群发测试2", true);
        
        QList<MessageRecord> records = db->getGroupMessageHistory(10);
        QVERIFY(records.size() >= 2);
    }

    // 管理员验证测试
    void testValidateAdmin_Success() {
        DbResult result = db->validateAdmin("admin", "Admin@123");
        QVERIFY2(result.success, qPrintable(result.errorMessage));
    }

    void testValidateAdmin_WrongPassword() {
        DbResult result = db->validateAdmin("admin", "wrongpassword");
        QVERIFY(!result.success);
    }

    void testValidateAdmin_NonExistent() {
        DbResult result = db->validateAdmin("nonexistent_admin", "Password1!");
        QVERIFY(!result.success);
    }
};

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
