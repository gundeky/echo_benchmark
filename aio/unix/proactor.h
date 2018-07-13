#ifndef __PROACTOR_H_CF935339_210F_4609_B153_3F9F220F4960__
#define __PROACTOR_H_CF935339_210F_4609_B153_3F9F220F4960__

#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <boost/function.hpp>
#include <vector>
#include <set>

#ifdef DEBUG
#include "cpl_log.h"
#endif

#ifndef IOVEC_MAX_COUNT
#define IOVEC_MAX_COUNT 10
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#ifndef INVALID_HANDLE
#define INVALID_HANDLE -1
#endif

#ifndef SAFE_CLOSE_SOCKET
#define SAFE_CLOSE_SOCKET(s) do{if (s!=INVALID_SOCKET) { ::close(s); s=INVALID_SOCKET; }}while(0)
#endif

#ifndef SAFE_CLOSE_HANDLE
#define SAFE_CLOSE_HANDLE(h) do{if (h!=INVALID_HANDLE) { ::close(h); h=INVALID_HANDLE; }}while(0)
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) do{if (p!=NULL) { delete(p); p=NULL; }}while(0)
#endif

typedef int SOCKET;

namespace aio
{

struct Error
{
	enum { ERRBUFSIZE = 128 };

	Error();
	Error(const Error& err);
	Error& operator= (const Error& err);
	void set(int code, const char* fmt, ...);
	void clear();

	int code;
	char detail[ERRBUFSIZE];
};

// callbacks
typedef boost::function<void(const Error&, int)> Callback;
typedef boost::function<void(const Error&, SOCKET)> AcceptCallback;

// internal use only
struct EventHandler
{
	EventHandler() {}
	virtual ~EventHandler() {}
	virtual void onRead()   = 0;
	virtual void onWrite()  = 0;
	virtual void onClose()  = 0;
	virtual SOCKET handle() = 0;
};

struct Token
{
	char* buf;       // 읽어야 할 버퍼
	int bufSize;     // 버퍼 사이즈
	int ioSize;      // 읽은 사이즈
	Callback cb;     // 다 끝나면 불러야 할 콜백
	char mode;       // N: none, S: some, A: all, V: v, D: done
	Error err;
	Token* prev;
	Token* next;
	struct timeval tv;

	static const char NONE = 'N';
	static const char SOME = 'S';
	static const char ALL  = 'A';
	static const char DONE = 'D';
	static const char ERR  = 'E';

	Token();
	void init(char* buf, int bufSize, Callback cb);
	void set(int ioSize, char mode);
	void add(int ioSize);

	struct LessTimer
	{
		bool operator() (Token* lhs, Token* rhs)
		{
			return timercmp(&(lhs->tv), &(rhs->tv), <);
		}
	};
};

class Proactor;    // forward declaration

class Socket : public EventHandler
{
	public:
		Socket(Proactor* proactor);
		Socket(SOCKET soc, Proactor* proactor);
		~Socket();

		// bool connect(const char* ip, unsigned short port, Callback cb);
		bool readSome(char* buf, int size, Callback cb, int timeoutMsec = 0);
		bool readAll(char* buf, int size, Callback cb, int timeoutMsec = 0);
		bool writeAll(char* buf, int size, Callback cb, int timeoutMsec = 0);
		// bool readvAll(const IoVec& iov, Callback cb);  // TODO iov 를 사용하지 말고, 각 파라미터 2개, 3개 함수를 작성
		// bool writevAll(const IoVec& iov, Callback cb); // TODO iov 를 사용하지 말고, 각 파라미터 2개, 3개 함수를 작성

		void close();

		SOCKET handle() { return _soc; }
		void handle(SOCKET soc) { _soc = soc; }

		// Error& error() { return _err; }

		// internal use
		void onRead();
		void onWrite();
		void onClose();

	private:
		Socket(const Socket& rhs);
		const Socket& operator= (const Socket& rhs);

	private:
		// bool _connect(const char* ip, unsigned short port);
		int _read(char* buf, int size);
		int _write(const char* buf, int size);
		// int _readv(IoVec& iov);  // TODO iov 를 사용하지 말고, 각 파라미터 2개, 3개 함수를 작성
		// int _writev(IoVec& iov); // TODO iov 를 사용하지 말고, 각 파라미터 2개, 3개 함수를 작성

	private:
		SOCKET _soc;
		Proactor* _proactor;
		Token _rt;
		Token _wt;
		// Error _err;
};

class Acceptor : public EventHandler
{
	public:
		Acceptor(Proactor* proactor);
		Acceptor(SOCKET soc, Proactor* proactor);
		~Acceptor();

		bool listen(const char* ip, unsigned short port, int backlog, AcceptCallback cb);

		void close();

		SOCKET handle() { return _soc; }
		void handle(SOCKET soc) { _soc = soc; }

		Error& error() { return _err; }

		// internal use
		void onRead();
		void onWrite();
		void onClose();

	private:
		bool _listen(const char* ip, unsigned short port, int backlog);
		SOCKET _accept(char* clientIp);

	private:
		Acceptor(const Acceptor& rhs);
		const Acceptor& operator= (const Acceptor& rhs);

	private:
		SOCKET _soc;
		Proactor* _proactor;
		AcceptCallback _cb;
		Error _err;
};

class Proactor
{
	private:
		enum { MAX_HANDLE = USHRT_MAX };

	public:
		Proactor();
		~Proactor() {}

		bool create(int pollIntervalMsec = 100);
		bool run();
		void stop();

		Error& error() { return _err; }

	private:
		bool _register(EventHandler* handler, bool acceptor = false);
		bool _remove(EventHandler* handler);
		void _enqueue(Token* token);
		void _remove(Token* token);
		void _processCompletion();
		void _pushTimer(Token* tkn, int timeoutMsec);
		void _popTimer(Token* token);
		void _processTimer();

	private:
		int _pollIntervalMsec;
		int _fd;
		bool _stop;
		Error _err;
		Token _head;
		std::vector<EventHandler*> _deleted;
		std::set<Token*, Token::LessTimer> _timerList;
		friend class Socket;
		friend class Acceptor;
};

}    // end of namespace aio

#endif /*__PROACTOR_H_CF935339_210F_4609_B153_3F9F220F4960__*/
