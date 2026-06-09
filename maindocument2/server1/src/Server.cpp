#include "Server.h"
#include"Session.h"
#include"RoomHandler.h"
#include"FileTransfer.h"

Server::Server(asio::io_context& io, uint16_t port)
	: io_(io),
	acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) ,
	roomHandler_(std::make_shared<RoomHandler>())
{
}

void Server::start_accept() {
	auto self = shared_from_this();

	acceptor_.async_accept([self](asio::error_code ec, asio::ip::tcp::socket socket_) {
        if (!ec)
        {
            std::make_shared<Session>(std::move(socket_), self->roomHandler_)->start();
        }
        else
        {
           
            if (ec == asio::error::operation_aborted)
                return;

            // Optional log
            std::cout << "accept error: " << ec.message() << "\n";
        }

        
        if (self->acceptor_.is_open())
            self->start_accept();
	});

}