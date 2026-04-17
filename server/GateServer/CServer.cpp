#include "CServer.h"
#include <iostream>
#include "HttpConnection.h"
#include "AsioIOServicePool.h"
// 构造函数：初始化 SSL 上下文
CServer::CServer(boost::asio::io_context& ioc, unsigned short& port)
    : _ioc(ioc),
    _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
    _ssl_ctx(boost::asio::ssl::context::tlsv12_server) // 指定使用 TLS v1.2
{
    // 配置 SSL 选项
    _ssl_ctx.set_options(
        boost::asio::ssl::context::default_workarounds
        | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::single_dh_use);

    // 加载证书文件 (server.crt) 和 私钥文件 (server.key)
    _ssl_ctx.use_certificate_chain_file("server.crt");
    _ssl_ctx.use_private_key_file("server.key", boost::asio::ssl::context::pem);
}

void CServer::Start()
{
    auto self = shared_from_this();
    auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();

    std::shared_ptr<HttpConnection> new_con = std::make_shared<HttpConnection>(io_context, _ssl_ctx);

    // acceptor 依然接收原始的 TCP socket
    _acceptor.async_accept(new_con->GetSocket(), [self, new_con](boost::system::error_code ec) {
        try {
            if (ec) {
                self->Start();
                return;
            }

            // 处理新连接：HttpsConnection 内部会先进行 async_handshake
            new_con->Start();

            // 继续监听
            self->Start();
        }
        catch (std::exception& exp) {
            std::cout << "Exception in CServer::Start: " << exp.what() << std::endl;
            self->Start();
        }
        });
}

