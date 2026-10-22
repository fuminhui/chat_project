# 分布式即时聊天系统

一个由 Qt 桌面客户端、C++ 后端服务和 Node.js 邮箱验证码服务组成的即时聊天项目。系统按网关、状态与聊天服务拆分，并使用 gRPC 完成服务间通信。

## 功能概览

- 用户注册、登录与邮箱验证码
- 好友申请、联系人管理与私聊
- 多聊天服务节点和状态服务
- Redis 会话/状态数据与 MySQL 业务数据

## 技术栈

- 客户端：C++、Qt、qmake
- 服务端：C++、Boost.Asio、gRPC、Protocol Buffers
- 验证码服务：Node.js、Nodemailer、`@grpc/grpc-js`
- 数据：MySQL、Redis

## 目录结构

```text
.
├── client/llfcchat/          # Qt 桌面客户端
├── server/GateServer/        # HTTP/TCP 接入网关
├── server/StatusServer/      # 用户状态与聊天节点分配
├── server/ChatServer/        # 即时消息服务
├── server/VarifyServer/      # Node.js 邮箱验证码服务
└── sql备份/                  # 数据库初始化/备份文件
```

## 运行准备

1. 安装 Qt、Visual Studio C++ 工具链、gRPC/Protobuf、MySQL、Redis 和 Node.js。
2. 导入 `sql备份/` 中的数据库文件。
3. 按本机环境修改各服务目录下的 `config.ini`，以及验证码服务的 `config.json`。
4. 在 `server/VarifyServer` 执行 `npm install`，再执行 `npm run serve`。
5. 依次启动 `StatusServer`、一个或多个 `ChatServer`、`GateServer`。
6. 使用 Qt Creator 打开 `client/llfcchat/llfcchat.pro`，构建并启动客户端。

## 配置安全

配置文件可能包含数据库、Redis 或邮件服务凭据。公开部署前请改用环境变量或本地私密配置，并立即轮换已经公开过的凭据。
