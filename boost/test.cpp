#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include "cpl_log.h"
cpl::Logger g_logger;

using boost::asio::ip::tcp;
using boost::system::error_code;
using boost::asio::deadline_timer;

class Client
{
	public:
		enum { BUFLEN = 1024 };

	public:
		Client(boost::asio::io_service& ioService) : _soc(ioService)
		{
		}

		void open()
		{
			_soc.async_read_some(boost::asio::buffer(_buf, BUFLEN)
					, boost::bind(&Client::onRead, this
						, boost::asio::placeholders::error
						, boost::asio::placeholders::bytes_transferred));
		}

		void onRead(const error_code& err, size_t byte_transferred)
		{
			if (err)
			{
				LOG_ERROR("onRead error [%d][%s]",
						err.value(), err.message().c_str());
				delete this;
				return;
			}

			_buf[byte_transferred] = 0;
			// LOG_INFO("read complete [%d][%s]", byte_transferred, _buf);
			boost::asio::async_write(_soc,
					boost::asio::buffer(_buf, byte_transferred)
					, boost::bind(&Client::onWrite, this,
						boost::asio::placeholders::error));
		}

		void onWrite(const error_code& err)
		{
			if (err)
			{
				LOG_ERROR("onWrite error [%d][%s]",
						err.value(), err.message().c_str());
				delete this;
				return;
			}

			_soc.async_read_some(boost::asio::buffer(_buf, BUFLEN)
					, boost::bind(&Client::onRead, this
						, boost::asio::placeholders::error
						, boost::asio::placeholders::bytes_transferred));
		}

		tcp::socket& peer() { return _soc; }

	private:
		tcp::socket _soc;
		char _buf[BUFLEN];
};

class Server
{
	private:
		boost::asio::ip::tcp::acceptor _acceptor;

		void newHandler()
		{
			Client* client = new Client(_acceptor.get_io_service());
			_acceptor.async_accept(client->peer(), boost::bind(&Server::onAccept, this, client
						, boost::asio::placeholders::error));
		}

	public:
		Server(boost::asio::io_service& ioService, short port)
			: _acceptor(ioService, tcp::endpoint(tcp::v4(), port))
		{
			newHandler();
		}

		void onAccept(Client* client, const error_code& err)
		{
			if (err)
			{
				LOG_ERROR("onAccept error [%d][%s]",
						err.value(), err.message().c_str());
				delete this;
				return;
			}

			// LOG_INFO("onAccept success [%s]",
			// 		client->peer().remote_endpoint().address().to_string().c_str());
			client->open();
			newHandler();
		}
};

int main(int argc, char* argv[])
{
	LOG_INFO("main start");

	short port = 9000;
	try
	{
		boost::asio::io_service ioService;
		Server server(ioService, port);
		ioService.run();
	}
	catch (std::exception& e)
	{
		LOG_ERROR("Unexpected error [%s]", e.what());
	}

	LOG_INFO("main end");
	return 0;
}

