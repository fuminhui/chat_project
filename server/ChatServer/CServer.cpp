#include "CServer.h"
#include <iostream>
#include "AsioIOServicePool.h"
#include "UserMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"

CServer::CServer(boost::asio::io_context& io_context, short port):_io_context(io_context), _port(port),
_acceptor(io_context, tcp::endpoint(tcp::v4(),port)), _timer(_io_context, std::chrono::seconds(60)), _heartbeat_timer(_io_context, std::chrono::seconds(1)) // 初始化定时器为1秒
{
	cout << "Server start success, listen on port : " << _port << endl;
	// 提前给时间轮分配 20 个空桶（代表 20 秒的超时容忍度）
	for (int i = 0; i < 20; ++i) {
		_time_wheel.emplace_back(Bucket());
	}

	// 启动时间轮转动
	StartHeartbeatTimer();
	StartAccept();
}

CServer::~CServer() {
	cout << "Server destruct listen on port : " << _port << endl;
	
}

void CServer::HandleAccept(shared_ptr<CSession> new_session, const boost::system::error_code& error){
	if (!error) {
		// 生成替身并绑定
		auto heartbeat_node = std::make_shared<HeartbeatNode>(new_session);
		new_session->SetHeartbeatNode(heartbeat_node);

		// 首次塞入时间轮
		InsertTimeWheel(heartbeat_node);

		new_session->Start();
		lock_guard<mutex> lock(_mutex);
		_sessions.insert(make_pair(new_session->GetSessionId(), new_session));
	}
	else {
		cout << "session accept failed, error is " << error.what() << endl;
	}

	StartAccept();
}

void CServer::StartAccept() {
	auto &io_context = AsioIOServicePool::GetInstance()->GetIOService();
	shared_ptr<CSession> new_session = make_shared<CSession>(io_context, this);
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, placeholders::_1));
}

//根据session 的id删除session，并移除用户和session的关联
void CServer::ClearSession(std::string session_id) {
	
	lock_guard<mutex> lock(_mutex);
	if (_sessions.find(session_id) != _sessions.end()) {
		auto uid = _sessions[session_id]->GetUserId();

		//移除用户和session的关联
		UserMgr::GetInstance()->RmvUserSession(uid, session_id);
	}

	_sessions.erase(session_id);
	
}

//根据用户获取session
shared_ptr<CSession> CServer::GetSession(std::string uuid) {
	lock_guard<mutex> lock(_mutex);
	auto it = _sessions.find(uuid);
	if (it != _sessions.end()) {
		return it->second;
	}
	return nullptr;
}

bool CServer::CheckValid(std::string uuid)
{
	lock_guard<mutex> lock(_mutex);
	auto it = _sessions.find(uuid);
	if (it != _sessions.end()) {
		return true;
	}
	return false;
}
void CServer::StartHeartbeatTimer() {
	auto self = shared_from_this();
	_heartbeat_timer.async_wait([self](const boost::system::error_code& ec) {
		if (!ec) {
			self->OnHeartbeatTimer();
		}
		});
}

// 每秒钟触发一次的“死神收割”逻辑
void CServer::OnHeartbeatTimer() {
	{
		std::lock_guard<std::mutex> lock(_wheel_mtx);

		// 核心魔法 1：把最前面（最老、经历了 20 秒的）那个桶弹出去并销毁！
		// 此时，桶里面所有的 shared_ptr<HeartbeatNode> 引用计数都会减 1。
		// 如果某个客户端 20 秒没发任何消息，它的替身计数就会在这里归 0。
		// 归 0 瞬间，自动触发 ~HeartbeatNode()，彻底关闭连接！
		_time_wheel.pop_front();

		// 在尾部追加一个新的空桶，准备迎接下一秒的新心跳
		_time_wheel.emplace_back(Bucket());
	}

	// 重新设置定时器，等待下一秒的滴答
	_heartbeat_timer.expires_after(std::chrono::seconds(1));
	StartHeartbeatTimer();
}

// 
void CServer::InsertTimeWheel(std::shared_ptr<HeartbeatNode> node) {
	std::lock_guard<std::mutex> lock(_wheel_mtx);
	// 永远把收到消息的替身，塞进时间轮的最末尾
	_time_wheel.back().insert(node);
}
