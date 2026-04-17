#pragma once
#include <boost/asio.hpp>
#include "CSession.h"
#include <memory.h>
#include <map>
#include <mutex>
#include <boost/asio/steady_timer.hpp>

using boost::asio::ip::tcp;
class CServer:public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& io_context, short port);
	~CServer();
	void ClearSession(std::string);
	//根据uid获取session
	shared_ptr<CSession> GetSession(std::string);
	bool CheckValid(std::string);
	//将心跳节点塞入时间轮
	void InsertTimeWheel(std::shared_ptr<HeartbeatNode> node);
	void on_timer(const boost::system::error_code& ec);
	// 启动并处理时间轮定时器
	void StartHeartbeatTimer();
	void OnHeartbeatTimer();
private:

	void HandleAccept(shared_ptr<CSession>, const boost::system::error_code & error);
	void StartAccept();
	boost::asio::io_context &_io_context;
	short _port;
	tcp::acceptor _acceptor;
	std::map<std::string, shared_ptr<CSession>> _sessions;
	std::mutex _mutex;
	boost::asio::steady_timer _timer;

	// 外层是 deque（20个槽位），内层是 unordered_set（每个槽位装一组心跳替身）
	using Bucket = std::unordered_set<std::shared_ptr<HeartbeatNode>>;
	std::deque<Bucket> _time_wheel;

	// 1秒走一格的滴答定时器
	boost::asio::steady_timer _heartbeat_timer;

	// 并发锁：保护时间轮，防止业务线程塞入和定时器线程弹出发生数据竞争
	std::mutex _wheel_mtx;
};

