#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
// #include <functional>
#include <boost/bind.hpp>
//#define ENABLE_MEMORYPOOL
//#define ENABLE_CHUNK
#include "proactor.h"
#include "cpl_log.h"

#define IP              "127.0.0.1"
// 9000 : readSome test
// 9001 : readAll  test
// 9002 : writeAll test
// 9003 : writeAll 2 test : send buffer full test
// 9004 : readvAll + writevAll test
// 9005 : connect test
#define PORT           9000

// #define TIMEOUT_MS     100 * 1000
#define TIMEOUT_MS     5 * 1000
// #define TIMEOUT_MS     0

// using namespace std::placeholders;
using namespace boost::placeholders;

class EchoClient
{
public:
	EchoClient(SOCKET soc, aio::Proactor* proactor) : _soc(soc, proactor)
	{
		LOG_INFO("---------- EchoClient ----------");
	}

	~EchoClient()
	{
		LOG_INFO("---------- ~EchoClient ----------");
		_soc.close();
	}

	void startRecv()
	{
		if (_soc.readSome(recvBuf, sizeof(recvBuf),
					boost::bind(&EchoClient::onRead, this, _1, _2), TIMEOUT_MS) == false)    // async recv 시작
		{
			// aio::Error& err = _soc.error();
			// LOG_INFO("recv fail [%d][%s]", err.code, err.detail);
			LOG_INFO("recv fail");
		}
	}

	void onRead(const aio::Error& err, int bytes)
	{
		LOG_INFO("---------- onRead start ----------");

		if (err.code != 0)
		{
			LOG_INFO("onRead - error [%d][%s]", err.code, err.detail);
			delete this;
			return;
		}
		if (bytes <= 0) {
			LOG_INFO("onRead - disconnected");
			delete this;
			return;
		}

		// recvBuf[bytes] = 0;
		// LOG_INFO("onRead [%s]", recvBuf);
		memcpy(sendBuf, recvBuf, bytes);

		if (_soc.writeAll(sendBuf, bytes, 
					boost::bind(&EchoClient::onWrite, this, _1, _2), TIMEOUT_MS) == false)
		{
			// aio::Error& err = _soc.error();
			// LOG_INFO("send fail [%d][%s]", err.code, err.detail);
			LOG_INFO("send fail");
		}

		LOG_INFO("---------- onRead end ----------");
	}

	void onWrite(const aio::Error& err, int bytes)
	{
		LOG_INFO("---------- onWrite start ----------");

		if (err.code != 0)
		{
			LOG_INFO("onWrite - error [%d][%s]", err.code, err.detail);
			delete this;
			return;
		}
		LOG_INFO(">>>>> 1");
		if (bytes <= 0)
		{
			LOG_INFO("onWrite - disconnected");
			delete this;
			return;
		}
		LOG_INFO(">>>>> 2 [%d]", bytes);

		// sendBuf[bytes] = 0;
		// LOG_INFO("onWrite [%s]", sendBuf);

		LOG_INFO(">>>>> 3");

		startRecv();    // 다음 데이터를 받기 위하여

		LOG_INFO(">>>>> 4");

		LOG_INFO("---------- onWrite end ----------");
	}

private:
	aio::Socket _soc;
	char recvBuf[1024];
	char sendBuf[1024];
};

class Server
{
public:
	Server(aio::Proactor& proactor) : _proactor(proactor), _acceptor(&proactor)
	{
		// LOG_INFO("---------- Server ----------");
	}

	~Server()
	{
		// LOG_INFO("---------- ~Server ----------");
	}

	bool startAccept()
	{
		if (_acceptor.listen(IP, PORT, SOMAXCONN,
					boost::bind(&Server::onAccept, this, _1, _2)) == false)    // listen 9000 ~ 9003
		{
			aio::Error& err = _acceptor.error();
			LOG_INFO("listen fail [%d][%s]", err.code, err.detail);
			return false;
		}

		return true;
	}

	void onAccept(const aio::Error&, SOCKET acceptSoc)
	{
		// LOG_INFO("---------- onAccept start ----------");

		EchoClient* client = new EchoClient(acceptSoc, &_proactor);
		client->startRecv();

		// LOG_INFO("---------- onAccept end ----------");
	}

	void close()
	{
		_acceptor.close();
	}

private:
	aio::Proactor& _proactor;
	aio::Acceptor _acceptor;
};

void SetSigHandler();
extern aio::Proactor* g_proactor;

int main(int argc, char* argv[])
{
#ifdef DEBUG
#else
	LOG_INIT(STDOUT_FILENO, cpl::Logger::LEVEL_ERROR);
#endif

	SetSigHandler();
	aio::Proactor proactor;
	g_proactor = &proactor;    // for signal stop

	if (proactor.create() == false)
	{
		LOG_INFO("Proactor::create fail [%d][%s]", proactor.error().code, proactor.error().detail);
		return 1;
	}

	Server server(proactor);
	if (server.startAccept() == false)
	{
		LOG_INFO("Server::startAccept fail");
		return 1;
	}

	if (proactor.run() == false)
	{
		LOG_INFO("Proactor open fail");
		return 1;
	}

	return 0;
}
