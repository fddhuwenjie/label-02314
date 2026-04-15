# Qt 聊天室服务端

基于 Qt6 的即时通讯服务器，支持多用户在线聊天、私聊、群聊功能。

## How to Run

### Docker 启动（推荐）

```bash
# 一键启动（自动安装依赖、构建、验证）
./start.sh

# 或手动启动
docker-compose up --build -d

# 使用 MySQL
docker-compose --profile mysql up --build -d

# 使用 PostgreSQL  
docker-compose --profile postgres up --build -d

# 查看日志
docker-compose logs -f chatserver

# 停止服务
docker-compose down
```

### 本地启动

```bash
# Ubuntu/Debian 安装依赖
sudo apt-get install qt6-base-dev libqt6sql6-sqlite cmake build-essential

# macOS
brew install qt6 cmake

# 编译
cd backend
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 运行
./build/ChatServer
```

### 运行测试

```bash
cd backend
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Services

| 服务 | 端口 | 说明 |
|------|------|------|
| ChatServer | 9999 | 聊天服务器主服务 |
| MySQL | 3306 | MySQL 数据库 (可选) |
| PostgreSQL | 5432 | PostgreSQL 数据库 (可选) |

## 测试账号

| 角色 | 用户名 | 密码 |
|------|--------|------|
| 管理员 | admin | Admin@123 |
| 普通用户 | - | 通过客户端注册 |

## 题目内容

我现在需要做一个基于QT的聊天室，分为客户端和服务端，现在你要制作一个服务端。以下是服务器端的功能。你需要根据功能编写代码实现。1：服务器端（40） 

（1）设计管理员登陆界面 

（2）保存所有用户的注册信息 

（3）负责转发信息 

（4）有用户登陆就在已在线用户界面显示其信息 

（5）可设置一次最多有多少个人在线，超过就无法登陆

---

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                      客户端 (TCP)                            │
└─────────────────────────┬───────────────────────────────────┘
                          │ JSON 协议
┌─────────────────────────▼───────────────────────────────────┐
│                     Server (QTcpServer)                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ 连接管理     │  │ 消息路由     │  │ 在线用户管理         │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│                    Database (QSqlDatabase)                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ 用户管理     │  │ 消息存储     │  │ 权限管理             │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────┬───────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        ▼                 ▼                 ▼
   ┌─────────┐      ┌─────────┐      ┌───────────┐
   │ SQLite  │      │  MySQL  │      │ PostgreSQL│
   └─────────┘      └─────────┘      └───────────┘
```


## 核心模块

| 模块 | 文件 | 职责 |
|------|------|------|
| Database | database.h/cpp | 数据持久化、用户认证、消息存储 |
| Server | server.h/cpp | TCP 连接管理、消息路由、在线用户管理 |
| LoginWindow | loginwindow.h/cpp | 管理员登录界面 |
| MainWindow | mainwindow.h/cpp | 服务器控制台界面 |

## 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| DB_TYPE | sqlite | 数据库类型 (sqlite/mysql/postgresql) |
| DB_HOST | localhost | 数据库主机 |
| DB_PORT | 3306/5432 | 数据库端口 |
| DB_NAME | chatserver | 数据库名称 |
| DB_USER | - | 数据库用户名 |
| DB_PASS | - | 数据库密码 |

## API 协议

所有通信使用 JSON 格式，通过 TCP 传输。

### 用户认证

**登录请求**
```json
{
  "type": "login",
  "username": "user1",
  "password": "Password1!"
}
```

**登录响应**
```json
{
  "type": "login_response",
  "success": true,
  "nickname": "用户昵称",
  "role": 0
}
```

**注册请求**
```json
{
  "type": "register",
  "username": "newuser",
  "password": "Password1!",
  "nickname": "新用户"
}
```

**注册响应**
```json
{
  "type": "register_response",
  "success": true,
  "message": "注册成功"
}
```

### 消息通信

**发送消息 (群发)**
```json
{
  "type": "chat",
  "to": "all",
  "message": "Hello everyone!"
}
```

**发送消息 (私聊)**
```json
{
  "type": "chat",
  "to": "user2",
  "message": "Hello user2!"
}
```

**接收消息**
```json
{
  "type": "chat",
  "from": "user1",
  "from_nickname": "用户1",
  "to": "user2",
  "message": "Hello!",
  "time": "2026-02-23 10:30:00"
}
```

### 在线用户

**获取在线用户**
```json
{"type": "get_online_users"}
```

**在线用户列表**
```json
{
  "type": "online_users",
  "users": [
    {"username": "user1", "nickname": "用户1", "role": 0},
    {"username": "user2", "nickname": "用户2", "role": 1}
  ]
}
```

### 历史记录

**获取历史消息**
```json
{
  "type": "get_history",
  "target": "user2",
  "limit": 50
}
```

**历史消息响应**
```json
{
  "type": "history_response",
  "messages": [
    {
      "from": "user1",
      "to": "user2",
      "message": "Hello",
      "time": "2026-02-23 10:00:00",
      "is_group": false
    }
  ]
}
```

## 安全特性

- 密码使用 SHA256 + 盐值进行 10000 轮迭代哈希
- 密码复杂度要求：8位以上，包含大小写字母、数字和特殊字符
- 用户角色权限管理 (User/Moderator/Admin)
- 参数化 SQL 查询防止注入
- 服务端入口层输入校验（空用户名/空密码等）

## curl 测试

服务器使用原始 TCP 协议，可通过 `nc` (netcat) 进行测试：

```bash
# 测试注册（正常）
echo '{"type":"register","username":"testuser","password":"Test@123456","nickname":"测试用户"}' | nc localhost 9999

# 测试注册（空用户名 - 应返回错误）
echo '{"type":"register","username":"","password":"Test@123456","nickname":"测试"}' | nc localhost 9999
# 预期返回: {"message":"用户名不能为空","success":false,"type":"register_response"}

# 测试登录（正常）
echo '{"type":"login","username":"testuser","password":"Test@123456"}' | nc localhost 9999

# 测试登录（空密码 - 应返回错误）
echo '{"type":"login","username":"testuser","password":""}' | nc localhost 9999
# 预期返回: {"message":"密码不能为空","success":false,"type":"login_response"}

# 获取在线用户列表
echo '{"type":"get_online_users"}' | nc localhost 9999
```

使用 Docker 环境测试：

```bash
# 进入容器测试（空用户名注册）
docker exec chat-server sh -c 'echo "{\"type\":\"register\",\"username\":\"\",\"password\":\"Test@123456\",\"nickname\":\"测试\"}" | timeout 2 nc 127.0.0.1 9999'
# 预期返回: {"message":"用户名不能为空","success":false,"type":"register_response"}

# 正常注册
docker exec chat-server sh -c 'echo "{\"type\":\"register\",\"username\":\"testuser\",\"password\":\"Test@123456\",\"nickname\":\"测试用户\"}" | timeout 2 nc 127.0.0.1 9999'
# 预期返回: {"success":true,"type":"register_response"}

# 正常登录
docker exec chat-server sh -c 'echo "{\"type\":\"login\",\"username\":\"testuser\",\"password\":\"Test@123456\"}" | timeout 2 nc 127.0.0.1 9999'
# 预期返回: {"nickname":"测试用户","role":0,"success":true,"type":"login_response"}
```

## 用户角色

| 角色 | 值 | 权限 |
|------|-----|------|
| User | 0 | 普通用户，可发送消息 |
| Moderator | 1 | 版主，可管理消息 |
| Admin | 2 | 管理员，完全权限 |

## 数据库表结构

**admins 表**
| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER | 主键 |
| username | VARCHAR(50) | 用户名 |
| password | VARCHAR(128) | 密码哈希 |
| salt | VARCHAR(32) | 盐值 |

**users 表**
| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER | 主键 |
| username | VARCHAR(50) | 用户名 |
| password | VARCHAR(128) | 密码哈希 |
| salt | VARCHAR(32) | 盐值 |
| nickname | VARCHAR(50) | 昵称 |
| role | INTEGER | 角色 |
| register_time | DATETIME | 注册时间 |

**messages 表**
| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER | 主键 |
| from_user | VARCHAR(50) | 发送者 |
| to_user | VARCHAR(50) | 接收者 |
| message | TEXT | 消息内容 |
| is_group | INTEGER | 是否群发 |
| timestamp | DATETIME | 发送时间 |

## 项目结构

```
.
├── backend/
│   ├── CMakeLists.txt      # 构建配置
│   ├── Dockerfile          # Docker 镜像
│   ├── docker-entrypoint.sh
│   ├── src/
│   │   ├── main.cpp        # 程序入口
│   │   ├── database.h/cpp  # 数据库模块
│   │   ├── server.h/cpp    # 服务器模块
│   │   ├── loginwindow.h/cpp
│   │   └── mainwindow.h/cpp
│   └── tests/
│       ├── CMakeLists.txt  # 测试构建配置
│       ├── test_database.cpp
│       └── test_server.cpp
├── docker-compose.yml
├── start.sh                # 一键启动脚本
└── README.md
```

## License

MIT
