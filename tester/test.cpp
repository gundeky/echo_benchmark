#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <string.h>
#include <vector>
#include <time.h>

#ifdef WIN32
#else
#include <unistd.h>
#include <signal.h>
#endif

#include "cpl_log.h"
cpl::Logger g_logger;

using boost::asio::ip::tcp;
using boost::system::error_code;
using boost::asio::deadline_timer;

struct Global
{
	int size;
	int count;
	int duration;
	std::string host;
	std::string port;
	char* buf;
	bool stop;
	int success;
	int fail;
	time_t startTime;
	int elapsedTime;

	Global() : size(0), count(0), duration(0), stop(false), success(0), fail(0)
	{
	}

	void setStartTime()
	{
		startTime = time(NULL);
	}

	void setElapsedTime()
	{
		elapsedTime = (int)(time(NULL) - startTime);
	}
} g_global;

class TestClient
{
	public:
		TestClient(boost::asio::io_service& ioService, tcp::resolver::iterator itr)
			: _soc(ioService)
		{
			_buf = new char[g_global.size];
			boost::asio::async_connect(_soc, itr,
					boost::bind(&TestClient::onConnect, this,
						boost::asio::placeholders::error));

		}

		~TestClient()
		{
			delete _buf;
		}

		void onConnect(const error_code& err)
		{
			if (err)
			{
				LOG_ERROR("onConnect error [%d][%s]",
						err.value(), err.message().c_str());
				++g_global.fail;
				delete this;
				return;
			}

			if (g_global.stop)
				return;

			boost::asio::async_write(_soc, boost::asio::buffer(g_global.buf,
						g_global.size)
					, boost::bind(&TestClient::onWrite, this,
						boost::asio::placeholders::error));
		}

		void onWrite(const error_code& err)
		{
			if (err)
			{
				LOG_ERROR("onWrite error [%d][%s]",
						err.value(), err.message().c_str());
				++g_global.fail;
				delete this;
				return;
			}

			if (g_global.stop)
				return;

			boost::asio::async_read(_soc, boost::asio::buffer(_buf, g_global.size)
					, boost::bind(&TestClient::onRead, this,
						boost::asio::placeholders::error));
		}

		void onRead(const error_code& err)
		{
			if (err)
			{
				LOG_ERROR("onRead error [%d][%s]",
						err.value(), err.message().c_str());
				++g_global.fail;
				delete this;
				return;
			}

			if (g_global.stop)
				return;

			++g_global.success;

			boost::asio::async_write(_soc, boost::asio::buffer(g_global.buf,
						g_global.size)
					, boost::bind(&TestClient::onWrite, this,
						boost::asio::placeholders::error));
		}

	private:
		tcp::socket _soc;
		char* _buf;
};

void PrintUsage(const char* argv0)
{
	printf("usage) %s  -s{test data bytes} -c{connection count} -d{duration seconds} -h{host ip} -p{host port}\n", argv0);
	printf("usage) %s  -s1024 -c20 -d5 -h127.0.0.1 -p9000\n", argv0);
}

void LoadCmdLineArgs(int argc, char* argv[])
{
	int opt;
	extern char *optarg;
	std::string temp;

	while ((opt = getopt(argc, argv, "s:c:d:h:p:")) != -1)
	{
		switch(opt)
		{
			case 's':
				g_global.size = atoi(optarg);
				break;
			case 'c':
				g_global.count = atoi(optarg);
				break;
			case 'd':
				g_global.duration = atoi(optarg);
				break;
			case 'h':
				g_global.host = optarg;
				break;
			case 'p':
				g_global.port = optarg;
				break;
		}
	}

	if (g_global.size == 0 || g_global.count == 0 || g_global.duration == 0
			|| g_global.host.empty() || g_global.port.empty()) {
		PrintUsage(argv[0]);
		exit(1);
	}

	LOG_INFO("test data bytes: %d", g_global.size);
	LOG_INFO("connection count: %d", g_global.count);
	LOG_INFO("duration seconds: %d", g_global.duration);
	LOG_INFO("host ip: %s", g_global.host.c_str());
	LOG_INFO("host port: %s", g_global.port.c_str());
}

void CreateBuffer()
{
	g_global.buf = new char[g_global.size];
	memset(g_global.buf, (int)'1', g_global.size);
}


boost::asio::io_service* g_ioService = NULL;

void OnTimer(const error_code& err)
{
	if (err)
	{
		LOG_ERROR("onTimer error [%d][%s]",
				err.value(), err.message().c_str());
		return;
	}

	g_global.stop = true;
	g_ioService->stop();
}

#ifndef WIN32
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

int main(int argc, char* argv[])
{
	LOG_INFO("main start");
#ifndef WIN32
	SetSigHandler();
#endif
	LoadCmdLineArgs(argc, argv);
	CreateBuffer();

	try
	{
		boost::asio::io_service ioService;
		g_ioService = &ioService;

		tcp::resolver resolver(ioService);
		tcp::resolver::query query(g_global.host, g_global.port);
		tcp::resolver::iterator itr = resolver.resolve(query);

		g_global.setStartTime();

		std::vector<TestClient*> vec;
		for (int i=0; i<g_global.count; ++i)
		{
			vec.push_back(new TestClient(ioService, itr));
		}

		deadline_timer timer(ioService);
		timer.expires_from_now(boost::posix_time::seconds(g_global.duration));
		timer.async_wait(OnTimer);

		ioService.run();

		for (int i=0; i<g_global.count; ++i)
		{
			delete vec[i];
		}

		g_global.setElapsedTime();
	}
	catch (std::exception& e)
	{
		LOG_ERROR("Unexpected error [%s]", e.what());
	}

	LOG_INFO("main end");
	LOG_INFO("success [%d] fail [%d] elapsed [%d] tps [%f]"
			, g_global.success, g_global.fail, g_global.elapsedTime
			, (float)(g_global.success + g_global.fail) / g_global.elapsedTime);

	return 0;
}

