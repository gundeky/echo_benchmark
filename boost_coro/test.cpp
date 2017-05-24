#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <string>

#include "cpl_log.h"
cpl::Logger g_logger;

#ifndef WIN32
#include <unistd.h>
#include <signal.h>

boost::asio::io_service* g_ioService = NULL;

void SigHandler(int nSig)
{
	switch(nSig)
	{
		case SIGINT:
			printf("SigHandler: SIGINT signal\n");
			break;

		case SIGTERM:
			printf("SigHandler: SIGTERM signal\n");
			break;

		// case SIGSEGV:
		// 	printf("SigHandler: SIGSEGV signal\n");
		// 	break;

		case SIGPIPE:
			printf("SigHandler: SIGPIPE signal\n");
			break;

		default:
			printf("SigHandler: %d signal\n", nSig);
			break;
	}
	g_ioService->stop();
}

void SetSigHandler()
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = SigHandler;
	sigfillset(&sa.sa_mask);
	sigaction(SIGINT , &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	// sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
}
#endif

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

		void run(boost::system::error_code err = boost::system::error_code(),
				std::size_t length = 0);

		tcp::socket& soc() { return _soc; }

	private:
		tcp::socket _soc;
		char _buf[BUFLEN];
		boost::asio::coroutine _coro;
};

class Server
{
	public:
		Server(boost::asio::io_service& ioService, short port)
			: _acceptor(ioService, tcp::endpoint(tcp::v4(), port)), _newClient(NULL)
		{
			// TODO why not working ?
			// tcp::resolver resolver(ioService);
			// tcp::resolver::query query(std::to_string(port));
			// _acceptor.bind(*resolver.resolve(query));
			// _acceptor.listen();
		}

		~Server()
		{
			if (_newClient != NULL)
				delete _newClient;
		}

		void run(boost::system::error_code err = boost::system::error_code());

	private:
		tcp::acceptor _acceptor;
		Client* _newClient;
		boost::asio::coroutine _coro;
};

#include <boost/asio/yield.hpp>
void Server::run(boost::system::error_code err)
{
	if (err)
	{
		LOG_ERROR("Server::run error [%d][%s]", err.value(), err.message().c_str());
		return;
	}

	reenter (_coro)
	{
		while(true)
		{
			_newClient = new Client(_acceptor.get_io_service());
			yield _acceptor.async_accept(_newClient->soc(), boost::bind(&Server::run,
						this, boost::asio::placeholders::error));
			_newClient->run();
		}
	}
}

void Client::run(boost::system::error_code err, std::size_t length)
{
	if (err)
	{
		LOG_ERROR("Client::run error [%d][%s]", err.value(), err.message().c_str());
		delete this;
		return;
	}

	reenter (_coro)
	{
		while(true)
		{
			yield _soc.async_read_some(boost::asio::buffer(_buf, BUFLEN)
					, boost::bind(&Client::run, this
						, boost::asio::placeholders::error
						, boost::asio::placeholders::bytes_transferred));

			// LOG_INFO("async_read_some: [%.*s][%zd]", length, _buf, length);

			yield boost::asio::async_write(_soc, boost::asio::buffer(_buf, length)
					, boost::bind(&Client::run, this
						, boost::asio::placeholders::error
						, boost::asio::placeholders::bytes_transferred));

			// LOG_INFO("async_write complete: %zd", length);
		}
	}
}
#include <boost/asio/unyield.hpp>

int main(int argc, char* argv[])
{
	LOG_INFO("main start");

	SetSigHandler();

	short port = 9000;
	try
	{
		boost::asio::io_service ioService;
		g_ioService = &ioService;
		Server server(ioService, port);
		server.run();
		ioService.run();
	}
	catch (std::exception& e)
	{
		LOG_ERROR("Unexpected error [%s]", e.what());
	}

	LOG_INFO("main end");
	return 0;
}

