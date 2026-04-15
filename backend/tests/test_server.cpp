#include <QtTest/QtTest>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include "../src/server.h"
#include "../src/database.h"

class TestServer : public QObject {
    Q_OBJECT

private:
    Database *db;
    Server *server;
    const quint16 TEST_PORT = 19999;

private slots:
    void initTestCase() {
        db = new Database();
        DbResult result = db->init("sqlite", "", 0, ":memory:", "", "");
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        
        // 注册测试用户
        db->registerUser("testuser1", "Password1!", "测试用户1");
        db->registerUser("testuser2", "Password1!", "测试用户2");
        
        server = new Server(db);
    }

    void cleanupTestCase() {
        server->stopServer();
        delete server;
        delete db;
    }

    // 服务器启动测试
    void testStartServer() {
        ServerResult result = server->startServer(TEST_PORT);
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        QVERIFY(server->isListening());
    }

    void testStartServer_PortInUse() {
        Server *server2 = new Server(db);
        ServerResult result = server2->startServer(TEST_PORT);
        QVERIFY(!result.success);
        delete server2;
    }

    // 最大在线人数设置测试
    void testSetMaxOnlineUsers() {
        server->setMaxOnlineUsers(50);
        QCOMPARE(server->maxOnlineUsers(), 50);
    }

    // 客户端连接测试
    void testClientConnection() {
        QTcpSocket client;
        client.connectToHost("127.0.0.1", TEST_PORT);
        QVERIFY(client.waitForConnected(3000));
        client.disconnectFromHost();
    }

    // 登录协议测试
    void testLoginProtocol_Success() {
        QTcpSocket client;
        client.connectToHost("127.0.0.1", TEST_PORT);
        QVERIFY(client.waitForConnected(3000));
        
        QJsonObject loginMsg;
        loginMsg["type"] = "login";
        loginMsg["username"] = "testuser1";
        loginMsg["password"] = "Password1!";
        
        client.write(QJsonDocument(loginMsg).toJson(QJsonDocument::Compact));
        client.flush();
        
        QVERIFY(client.waitForReadyRead(3000));
        QByteArray response = client.readAll();
        QJsonObject responseJson = QJsonDocument::fromJson(response).object();
        
        QCOMPARE(responseJson["type"].toString(), QString("login_response"));
        QVERIFY(responseJson["success"].toBool());
        
        client.disconnectFromHost();
    }

    void testLoginProtocol_WrongPassword() {
        QTcpSocket client;
        client.connectToHost("127.0.0.1", TEST_PORT);
        QVERIFY(client.waitForConnected(3000));
        
        QJsonObject loginMsg;
        loginMsg["type"] = "login";
        loginMsg["username"] = "testuser2";
        loginMsg["password"] = "WrongPassword1!";
        
        client.write(QJsonDocument(loginMsg).toJson(QJsonDocument::Compact));
        client.flush();
        
        QVERIFY(client.waitForReadyRead(3000));
        QByteArray response = client.readAll();
        QJsonObject responseJson = QJsonDocument::fromJson(response).object();
        
        QCOMPARE(responseJson["type"].toString(), QString("login_response"));
        QVERIFY(!responseJson["success"].toBool());
        
        client.disconnectFromHost();
    }

    // 注册协议测试
    void testRegisterProtocol_Success() {
        QTcpSocket client;
        client.connectToHost("127.0.0.1", TEST_PORT);
        QVERIFY(client.waitForConnected(3000));
        
        QJsonObject registerMsg;
        registerMsg["type"] = "register";
        registerMsg["username"] = "newuser_proto";
        registerMsg["password"] = "Password1!";
        registerMsg["nickname"] = "新用户协议测试";
        
        client.write(QJsonDocument(registerMsg).toJson(QJsonDocument::Compact));
        client.flush();
        
        QVERIFY(client.waitForReadyRead(3000));
        QByteArray response = client.readAll();
        QJsonObject responseJson = QJsonDocument::fromJson(response).object();
        
        QCOMPARE(responseJson["type"].toString(), QString("register_response"));
        QVERIFY(responseJson["success"].toBool());
        
        client.disconnectFromHost();
    }

    // 在线用户列表测试
    void testGetOnlineUsers() {
        // 先登录一个用户
        QTcpSocket client;
        client.connectToHost("127.0.0.1", TEST_PORT);
        QVERIFY(client.waitForConnected(3000));
        
        QJsonObject loginMsg;
        loginMsg["type"] = "login";
        loginMsg["username"] = "testuser2";
        loginMsg["password"] = "Password1!";
        client.write(QJsonDocument(loginMsg).toJson(QJsonDocument::Compact));
        client.flush();
        QVERIFY(client.waitForReadyRead(3000));
        client.readAll(); // 清空登录响应
        
        // 请求在线用户列表
        QJsonObject getUsersMsg;
        getUsersMsg["type"] = "get_online_users";
        client.write(QJsonDocument(getUsersMsg).toJson(QJsonDocument::Compact));
        client.flush();
        
        QVERIFY(client.waitForReadyRead(3000));
        QByteArray response = client.readAll();
        QJsonObject responseJson = QJsonDocument::fromJson(response).object();
        
        QCOMPARE(responseJson["type"].toString(), QString("online_users"));
        QVERIFY(responseJson.contains("users"));
        
        client.disconnectFromHost();
    }

    // 信号测试
    void testUserLoggedInSignal() {
        QSignalSpy spy(server, &Server::userLoggedIn);
        
        QTcpSocket client;
        client.connectToHost("127.0.0.1", TEST_PORT);
        QVERIFY(client.waitForConnected(3000));
        
        // 注册并登录新用户
        db->registerUser("signaltest", "Password1!", "信号测试");
        
        QJsonObject loginMsg;
        loginMsg["type"] = "login";
        loginMsg["username"] = "signaltest";
        loginMsg["password"] = "Password1!";
        client.write(QJsonDocument(loginMsg).toJson(QJsonDocument::Compact));
        client.flush();
        
        QVERIFY(client.waitForReadyRead(3000));
        QVERIFY(spy.count() >= 1);
        
        client.disconnectFromHost();
    }

    void testStopServer() {
        server->stopServer();
        QVERIFY(!server->isListening());
    }
};

QTEST_MAIN(TestServer)
#include "test_server.moc"
