#pragma once
#include "const.h"


class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
	friend class LogicSystem;
public:
	
	HttpConnection(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx);

	void Start();
	void PreParseGetParam();

	
	tcp::socket& GetSocket() {
		return _ssl_stream.lowest_layer();
	}

private:
	void DoRead(); 
	void CheckDeadline();
	void WriteResponse();
	void HandleReq();


	boost::asio::ssl::stream<boost::asio::ip::tcp::socket> _ssl_stream;

	// The buffer for performing reads.
	beast::flat_buffer _buffer{ 8192 };

	// The request message.
	http::request<http::dynamic_body> _request;

	// The response message.
	http::response<http::dynamic_body> _response;


	// The timer for putting a deadline on connection processing.
	net::steady_timer deadline_{
		_ssl_stream.get_executor(), std::chrono::seconds(60) };

	std::string _get_url;
	std::unordered_map<std::string, std::string> _get_params;
};