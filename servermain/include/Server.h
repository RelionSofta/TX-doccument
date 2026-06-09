#pragma once
#include<asio.hpp>
#include<cstdint>
class RoomHandler;

class Server :public std::enable_shared_from_this<Server>{
public:
	Server(asio::io_context & io , uint16_t port);
	void start_accept();

private:
	asio::io_context &io_;
	asio::ip::tcp::acceptor acceptor_;
	std::shared_ptr<RoomHandler> roomHandler_;
};
