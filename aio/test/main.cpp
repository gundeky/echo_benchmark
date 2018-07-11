#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <functional>
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

using namespace std::placeholders;

class EchoClient
{
public:
	EchoClient(SOCKET soc, aio::Proactor* proactor, int mode) : _proactor(proactor), _soc(soc, proactor), _mode(mode)
	{
		LOG_INFO("---------- EchoClient ----------");
	}

	~EchoClient()
	{
		LOG_INFO("---------- ~EchoClient ----------");
		_soc.close();
		for (size_t i=0; i<_socArray.size(); ++i)
		{
			aio::Socket* soc = _socArray[i];
			soc->close();
			delete soc;
		}
	}

	void startRecv()
	{
		if (_mode == 0)
		{
			if (_soc.readSome(recvBuf, sizeof(recvBuf),
						std::bind(&EchoClient::onRead, this, _1, _2)) == false)    // async recv 시작
			{
				aio::Error& err = _soc.error();
				LOG_INFO("recv fail [%d][%s]", err.code, err.detail);
			}
			else
			{
				LOG_INFO("recv invoke success");
			}
		}
		else if (_mode == 4)
		{
			startReadv();
		}
		else if (_mode == 5)
		{
			startConnect();
		}
		else    // _mode == 1
		{
			int readLen = 76;    // req message size
			if (_soc.readAll(recvBuf, readLen,
						std::bind(&EchoClient::onRead, this, _1, _2)) == false)    // async recv 시작
			{
				aio::Error& err = _soc.error();
				LOG_INFO("recv fail [%d][%s]", err.code, err.detail);
			}
			else
			{
				LOG_INFO("recv invoke success");
			}
		}
	}

	void onRead(aio::Error& err, int bytes)
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

		recvBuf[bytes] = 0;
		LOG_INFO("onRead [%s]", recvBuf);
		memcpy(sendBuf, recvBuf, bytes);

		if (_mode == 2)
		{
			LOG_DEBUG("enter sleep");
			sleep(1);
		}

		if (_mode == 3)    // TODO 버퍼를 꽉 채운다
		{
			int optval = 0;
			socklen_t optsize = sizeof(optval);
			if (getsockopt(_soc.handle(), SOL_SOCKET, SO_SNDBUF, (char*)&optval, &optsize) == -1)
			{
				LOG_ERROR("setsockopt(SO_SNDBUF) failed %d/%s", errno, strerror(errno));
				return;
			}
			LOG_INFO("sndbuf %d", optsize);
			optval = (int)1;
			if (setsockopt(_soc.handle(), SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval)) == -1)
			{
				LOG_ERROR("setsockopt(SO_SNDBUF) failed %d/%s", errno, strerror(errno));
				return;
			}
			optval = 0;
			optsize = sizeof(optval);
			if (getsockopt(_soc.handle(), SOL_SOCKET, SO_SNDBUF, (char*)&optval, &optsize) == -1)
			{
				LOG_ERROR("setsockopt(SO_SNDBUF) failed %d/%s", errno, strerror(errno));
				return;
			}
			LOG_INFO("sndbuf %d", optsize);

			int sendCount = 0;
			while (1)
			{
				char buf[20480];
				memset(buf, 1, sizeof(buf));
				int ret = ::send(_soc.handle(), buf, sizeof(buf), 0);
				if (ret <= 0)
				{
					if (errno == EAGAIN)
					{
						LOG_ERROR("onRead - send EAGAIN");
						break;
					}
					else
					{
						LOG_ERROR("onRead - send error [%d][%s]", errno, strerror(errno));
						break;
					}
				}
				LOG_DEBUG("send %d %d", ret, ++sendCount);
			}
		}

		if (_soc.writeAll(sendBuf, bytes, this, &EchoClient::onWrite) == false)
		{
			aio::Error& err = _soc.error();
			LOG_INFO("send fail [%d][%s]", err.code, err.detail);
		}

		LOG_INFO("---------- onRead end ----------");
	}

	void onWrite(aio::Error& err, int bytes)
	{
		LOG_INFO("---------- onWrite start ----------");

		if (err.code != 0)
		{
			LOG_INFO("onWrite - error [%d][%s]", err.code, err.detail);
			delete this;
			return;
		}
		if (bytes <= 0)
		{
			LOG_INFO("onWrite - disconnected");
			delete this;
			return;
		}

		sendBuf[bytes] = 0;
		LOG_INFO("onWrite [%s]", sendBuf);

		startRecv();    // 다음 데이터를 받기 위하여

		LOG_INFO("---------- onWrite end ----------");
	}

	// for iov test
	void startReadv()
	{
		memset(recvHeader, 0, sizeof(recvHeader));
		memset(recvBuf, 0, sizeof(recvBuf));

		aio::IoVec iov;
		iov.pushBack(recvHeader, 2);
		iov.pushBack(recvBuf, 2);

		if (_soc.readvAll(iov, this, &EchoClient::onReadv) == false)
		{
			aio::Error& err = _soc.error();
			LOG_INFO("recv fail [%d][%s]", err.code, err.detail);
		}
		else
		{
			LOG_INFO("recv invoke success");
		}
	}

	void onReadv(aio::Error& err, int bytes)
	{
		LOG_INFO("---------- onReadv start ----------");

		if (err.code != 0)
		{
			LOG_INFO("onReadv - error [%d][%s]", err.code, err.detail);
			delete this;
			return;
		}
		if (bytes <= 0) {
			LOG_INFO("onReadv - disconnected");
			delete this;
			return;
		}

		LOG_INFO("onReadv [%d][%s][%s]", bytes, recvHeader, recvBuf);
		memcpy(sendBuf, recvBuf, sizeof(sendBuf));
		memcpy(sendHeader, recvHeader, sizeof(sendHeader));

		aio::IoVec iov;
		iov.pushBack(sendHeader, 2);
		iov.pushBack(sendBuf, 2);

		if (_soc.writevAll(iov, this, &EchoClient::onWritev) == false)
		{
			aio::Error& err = _soc.error();
			LOG_INFO("writevAll fail [%d][%s]", err.code, err.detail);
		}

		LOG_INFO("---------- onReadv end ----------");
	}

	void onWritev(aio::Error& err, int bytes)
	{
		LOG_INFO("---------- onWritev start ----------");

		if (err.code != 0)
		{
			LOG_INFO("onWritev - error [%d][%s]", err.code, err.detail);
			delete this;
			return;
		}
		if (bytes <= 0)
		{
			LOG_INFO("onWritev - disconnected");
			delete this;
			return;
		}

		LOG_INFO("onWritev [%d]", bytes);

		startReadv();

		LOG_INFO("---------- onWritev end ----------");
	}

	void startConnect()
	{
		LOG_INFO("---------- startConnect start [%zu] ----------", _socArray.size());
		aio::Socket* soc = new aio::Socket(_proactor);
		if (soc->connect("127.0.0.1", 10000, this, &EchoClient::onConnect) == false)
		{
			LOG_ERROR("connect error [%d][%s]", soc->error().code, soc->error().detail);
			return;
		}
		_socArray.push_back(soc);
		LOG_INFO("---------- startConnect end [%zu] ----------", _socArray.size());
	}

	void onConnect(aio::Error& err)
	{
		LOG_INFO("---------- onConnect start [%zu] ----------", _socArray.size());
		if (err.code != 0)
		{
			LOG_INFO("onConnect - error [%d][%s]", err.code, err.detail);
			delete this;
			return;
		}

		LOG_INFO("connect success [%d]", _socArray.size());

		/*if (_socArray.size() < 10)
		{
			LOG_INFO("connect one more");*/
			startConnect();
			LOG_INFO("---------- onConnect end [%d] ----------", _socArray.size());
		//}
		/*else
		{
			LOG_INFO("no more connect delete this");
			delete this;
		}*/
	}

//private:
public:
	aio::Proactor* _proactor;
	aio::Socket _soc;
	std::vector<aio::Socket*> _socArray;
	char recvBuf[1024];
	char sendBuf[1024];
	char recvHeader[3];    // 1 for null termination
	char sendHeader[3];
	// 0 : readSome test
	// 1 : readAll  test
	// 2 : writeAll test
	// 3 : writeAll 2 test : send buffer full test
	// 4 : readvAll + writevAll test
	int _mode;
};

class Server
{
public:
	Server(aio::Proactor& proactor, int mode) : _proactor(proactor), _acceptor(&proactor), _mode(mode)
	{
		LOG_INFO("---------- Server ----------");
	}

	~Server()
	{
		LOG_INFO("---------- ~Server ----------");
	}

	bool startAccept()
	{
		if (_acceptor.listen(IP, PORT + _mode, SOMAXCONN,
					std::bind(&Server::onAccept, this)) == false)    // listen 9000 ~ 9003
		{
			aio::Error& err = _acceptor.error();
			LOG_INFO("listen fail [%d][%s]", err.code, err.detail);
			return false;
		}

		return true;
	}

	void onAccept(aio::Error&, SOCKET acceptSoc)
	{
		LOG_INFO("---------- onAccept start ----------");

		EchoClient* client = new EchoClient(acceptSoc, &_proactor, _mode);
		client->startRecv();

		LOG_INFO("---------- onAccept end ----------");
	}

	void close()
	{
		_acceptor.close();
	}

private:
	aio::Proactor& _proactor;
	aio::Acceptor _acceptor;
	int _mode;
};

void SetSigHandler();
extern aio::Proactor* g_proactor;

int main(int argc, char* argv[])
{
	SetSigHandler();
	aio::Proactor proactor;
	g_proactor = &proactor;    // for signal stop

#ifdef WIN32
	if (proactor.create(1) == false)
#else
	if (proactor.create() == false)
#endif
	{
		LOG_INFO("Proactor::create fail [%d][%s]", proactor.error().code, proactor.error().detail);
		return 1;
	}

	Server server0(proactor, 0);
	Server server1(proactor, 1);
	Server server2(proactor, 2);
	Server server3(proactor, 3);
	Server server4(proactor, 4);
	Server server5(proactor, 5);
	if (server0.startAccept() == false ||
		server1.startAccept() == false ||
		server2.startAccept() == false ||
		server3.startAccept() == false ||
		server4.startAccept() == false ||
		server5.startAccept() == false)
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
