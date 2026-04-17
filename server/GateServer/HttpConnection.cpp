#include "HttpConnection.h"
#include "LogicSystem.h"

HttpConnection::HttpConnection(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx)
	: _ssl_stream(ioc, ctx) {}

// Start 函数专门用于 SSL 握手
void HttpConnection::Start()
{
	auto self = shared_from_this();
	// 作为服务端发起 SSL 握手
	_ssl_stream.async_handshake(
		boost::asio::ssl::stream_base::server,
		[self](beast::error_code ec) {
			if (ec) {
				std::cout << "SSL handshake err is " << ec.message() << std::endl;
				return;
			}
			// 握手成功后，底层安全隧道建立，开始读取 HTTP 数据
			self->DoRead();
		}
	);
}

// 提取原来 Start 中的读取逻辑到独立的 DoRead 函数
void HttpConnection::DoRead()
{
	auto self = shared_from_this();
	http::async_read(_ssl_stream, _buffer, _request, [self](beast::error_code ec,
		std::size_t bytes_transferred) {
			try {
				if (ec) {
					std::cout << "http read err is " << ec.message() << std::endl;
					return;
				}

				//处理读到的数据
				boost::ignore_unused(bytes_transferred);
				self->HandleReq();
				self->CheckDeadline();
			}
			catch (std::exception& exp) {
				std::cout << "exception is " << exp.what() << std::endl;
			}
		}
	);
}

// char 转为 16 进制
unsigned char ToHex(unsigned char x)
{
	return  x > 9 ? x + 55 : x + 48;
}

// 16 进制转为 char
unsigned char FromHex(unsigned char x)
{
	unsigned char y;
	if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
	else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
	else if (x >= '0' && x <= '9') y = x - '0';
	else assert(0);
	return y;
}

// UrlEncode 
std::string UrlEncode(const std::string& str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		if (isalnum((unsigned char)str[i]) ||
			(str[i] == '-') ||
			(str[i] == '_') ||
			(str[i] == '.') ||
			(str[i] == '~'))
			strTemp += str[i];
		else if (str[i] == ' ')
			strTemp += "+";
		else
		{
			strTemp += '%';
			strTemp += ToHex((unsigned char)str[i] >> 4);
			strTemp += ToHex((unsigned char)str[i] & 0x0F);
		}
	}
	return strTemp;
}

// UrlDecode 无修改
std::string UrlDecode(const std::string& str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		if (str[i] == '+') strTemp += ' ';
		else if (str[i] == '%')
		{
			assert(i + 2 < length);
			unsigned char high = FromHex((unsigned char)str[++i]);
			unsigned char low = FromHex((unsigned char)str[++i]);
			strTemp += high * 16 + low;
		}
		else strTemp += str[i];
	}
	return strTemp;
}

// PreParseGetParam 无修改
void HttpConnection::PreParseGetParam() {
	auto uri = _request.target();
	auto query_pos = uri.find('?');
	if (query_pos == std::string::npos) {
		_get_url = uri;
		return;
	}

	_get_url = uri.substr(0, query_pos);
	std::string query_string = uri.substr(query_pos + 1);
	std::string key;
	std::string value;
	size_t pos = 0;
	while ((pos = query_string.find('&')) != std::string::npos) {
		auto pair = query_string.substr(0, pos);
		size_t eq_pos = pair.find('=');
		if (eq_pos != std::string::npos) {
			key = UrlDecode(pair.substr(0, eq_pos));
			value = UrlDecode(pair.substr(eq_pos + 1));
			_get_params[key] = value;
		}
		query_string.erase(0, pos + 1);
	}
	if (!query_string.empty()) {
		size_t eq_pos = query_string.find('=');
		if (eq_pos != std::string::npos) {
			key = UrlDecode(query_string.substr(0, eq_pos));
			value = UrlDecode(query_string.substr(eq_pos + 1));
			_get_params[key] = value;
		}
	}
}

// HandleReq 无修改
void HttpConnection::HandleReq() {
	_response.version(_request.version());
	_response.keep_alive(false);
	_response.set(boost::beast::http::field::access_control_allow_origin, "*");

	if (_request.method() == http::verb::get) {
		PreParseGetParam();
		bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
		if (!success) {
			_response.result(http::status::not_found);
			_response.set(http::field::content_type, "text/plain");
			beast::ostream(_response.body()) << "url not found\r\n";
			WriteResponse();
			return;
		}

		_response.result(http::status::ok);
		_response.set(http::field::server, "GateServer");
		WriteResponse();
		return;
	}

	if (_request.method() == http::verb::post) {
		bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
		if (!success) {
			_response.result(http::status::not_found);
			_response.set(http::field::content_type, "text/plain");
			beast::ostream(_response.body()) << "url not found\r\n";
			WriteResponse();
			return;
		}

		_response.result(http::status::ok);
		_response.set(http::field::server, "GateServer");
		WriteResponse();
		return;
	}
}

// 利用 lowest_layer() 获取底层 Socket 并执行 close
void HttpConnection::CheckDeadline() {
	auto self = shared_from_this();

	deadline_.async_wait(
		[self](beast::error_code ec)
		{
			if (!ec)
			{
				// 取出最底层的 tcp::socket 执行物理关闭
				self->_ssl_stream.lowest_layer().close(ec);
			}
		});
}

// 写入数据使用 _ssl_stream，断开连接使用 lowest_layer()
void HttpConnection::WriteResponse() {
	auto self = shared_from_this();

	_response.content_length(_response.body().size());

	// 写入时必须传递 _ssl_stream，让数据经过 SSL 层的加密处理
	http::async_write(
		_ssl_stream,
		_response,
		[self](beast::error_code ec, std::size_t)
		{
			// 发送完毕后，关闭底层的物理连接
			self->_ssl_stream.lowest_layer().shutdown(tcp::socket::shutdown_both, ec);
			self->deadline_.cancel();
		});
}