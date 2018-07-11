#include "proactor.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>

namespace aio
{

void MakeSockAddrIn(const char* ip, unsigned short port, struct sockaddr_in* addr)
{       
	addr->sin_family      = AF_INET;
	addr->sin_addr.s_addr = (ip != NULL ? ::inet_addr(ip) : htonl(INADDR_ANY));
	addr->sin_port        = htons((unsigned short)port);
}

bool Addr2str(struct sockaddr_in* addr, char* str)
{
	char temp[50];
	memset(temp, 0, sizeof(temp));
	const char* ret = inet_ntop(AF_INET, &(addr->sin_addr.s_addr), temp, sizeof(temp));
	if (ret == NULL)
		return false;
	strcpy(str, temp);
	return true;
}

bool SetNonBlock(int soc, bool nonblock, Error* err)
{
	if (soc == -1)
	{
		err->set(-1, "invalid fd");
		return false;
	}

	int flags = ::fcntl(soc, F_GETFL, 0);
	if(-1 == flags)
	{           
		err->set(errno, "fcntl fail [%d][%s]", errno, strerror(errno));
		return false;
	}   
	if (true == nonblock) 
	{       
		flags |= O_NONBLOCK;
	}
	else
	{
		flags &= ~O_NONBLOCK;
	}
	if (-1 == ::fcntl(soc, F_SETFL, flags))
	{
		err->set(errno, "fcntl fail [%d][%s]", errno, strerror(errno));
		return false;
	}
	return true;
}

Error::Error() : code(0)
{
	memset(detail, 0, ERRBUFSIZE);
}

Error::Error(const Error& err) : code(err.code)
{
	memcpy(detail, err.detail, ERRBUFSIZE);
}

Error& Error::operator= (const Error& err)
{
	code = err.code;
	memcpy(detail, err.detail, ERRBUFSIZE);
	return *this;
}

void Error::set(int code, const char* fmt, ...)
{
	this->code = code;
	va_list args;
	va_start(args, fmt);
	::vsnprintf(this->detail, sizeof(this->detail), fmt, args);
	va_end(args);
}

void Error::clear()
{
	code = 0;
	detail[0] = 0;
}

// IoVec::IoVec() : _count(0), _totalSize(0), _idx(0), _ioSize(0)
// {
// 	memset(_iov, 0, sizeof(_iov));
// }
//
// IoVec::IoVec(const IoVec& rhs)
// {
// 	copy(rhs);
// }
//
// bool IoVec::pushBack(void* base, size_t len)
// {
// 	if (_count >= IOVEC_MAX_COUNT)
// 		return false;
// 	_iov[_count].iov_base = base;
// 	_iov[_count].iov_len  = len ;
// 	++_count;
// 	_totalSize += (int)len;
// 	return true;
// }
//
// bool IoVec::set(int idx, void* base, size_t len)
// {
// 	if (idx >= _count)
// 		return false;
// 	_totalSize -= (int)_iov[idx].iov_len;
// 	_iov[idx].iov_base = base;
// 	_iov[idx].iov_len  = len ;
// 	_totalSize += (int)len;
// 	return true;
// }
//
// void IoVec::copy(const IoVec& rhs)
// {
// 	_count     = rhs._count;
// 	_totalSize = rhs._totalSize;
// 	_idx       = rhs._idx;
// 	_ioSize    = rhs._ioSize;
// 	for (int i=0; i<_count; ++i)
// 	{
// 		_iov[i].iov_base = rhs._iov[i].iov_base;
// 		_iov[i].iov_len  = rhs._iov[i].iov_len ;
// 	}
// }
//
// void IoVec::addIoSize(int ioSize)
// {
// 	_ioSize += ioSize;
// 	while (ioSize > 0)
// 	{
// 		struct iovec& iov = _iov[_idx];
// 		if (ioSize >= (int)iov.iov_len)
// 		{
// 			ioSize -= (int)iov.iov_len;
// 			++_idx;
// 		}
// 		else
// 		{
// 			iov.iov_base = (void*)((char*)iov.iov_base + ioSize);
// 			iov.iov_len  = (size_t)((int)iov.iov_len - ioSize);
// 			break;
// 		}
// 	}
// }
//
// struct iovec* IoVec::iov()
// {
// 	if (_idx < _count)
// 		return _iov + _idx;
// 	return NULL;
// }


Socket::Socket(Proactor* proactor) : _soc(INVALID_SOCKET), _proactor(proactor)
{
	//_proactor->_register(this);    // connect 에서 한다
}

Socket::Socket(SOCKET soc, Proactor* proactor) : _soc(soc), _proactor(proactor)
{
	_proactor->_register(this);
}

Socket::~Socket()
{
	if (_soc != INVALID_SOCKET)
		close();
	if (_rt.prev != NULL)
		_proactor->_remove(&_rt);
	if (_wt.prev != NULL)
		_proactor->_remove(&_wt);
}

// bool Socket::connect(const char* ip, unsigned short port, T* t, Callback cb)
// {
// #ifdef DEBUG
// 	LOG_DEBUG("connect 1");
// #endif
// 	if (_connACT)
// 	{
// 		_err.set(-1, "_connACT is not null");
// 		return false;
// 	}
//
// 	if (_connect(ip, port) == false)
// 		return false;
// #ifdef DEBUG
// 	LOG_DEBUG("connect [fd:%d][%p][%p]", _soc, t, f);
// #endif
// 	_connACT = new ConnACT<T>(t, f);
//
// 	if (_proactor->_register(this) == false)
// 	{
// 		_err = _proactor->_err;
// #ifdef DEBUG
// 	LOG_ERROR("proactor register error [%d][%s]", _err.code, _err.detail);
// #endif
// 		return false;
// 	}
// 	return true;
// }

bool Socket::readSome(char* buf, int size, Callback cb)
{
#ifdef DEBUG
	LOG_DEBUG("readSome 1 [%d]", _soc);
#endif
	if (_rt.mode != Token::NONE)
	{
		_rt.err.set(-1, "read in progress");
		return false;
	}

	_rt.init(buf, size, cb);
	int ret = _read(buf, size);
	if (ret == 0)    // 접속 종료
	{
#ifdef DEBUG
		LOG_DEBUG("readSome 2 [%d]", _soc);
#endif
		_rt.set(0, Token::DONE);    // read 0 후 끊어진다
		_proactor->_enqueue(&_rt);
	}
	else if (ret < 0)
	{
		if (_rt.err.code == EAGAIN)
		{
#ifdef DEBUG
			LOG_DEBUG("readSome 3 [%d]", _soc);
#endif
			_rt.set(0, Token::SOME);    // onRead 에서 더 읽는다
		}
		else
		{
#ifdef DEBUG
			LOG_DEBUG("readSome 4 [%d]", _soc);
#endif
			_rt.set(0, Token::ERR);  // 컴플리션 핸들러에 _err 전달
			_proactor->_enqueue(&_rt);
		}
	}
	else
	{
#ifdef DEBUG
		LOG_DEBUG("readSome 5 [%d]", _soc);
#endif
		_rt.set(ret, Token::DONE);    // 읽은 양 전달
		_proactor->_enqueue(&_rt);
	}
	return true;
}

bool Socket::readAll(char* buf, int size, Callback cb)
{
#ifdef DEBUG
	LOG_DEBUG("readAll 1 [%d]", _soc);
#endif
	if (_rt.mode != Token::NONE)
	{
		_rt.err.set(-1, "read in progress");
		return false;
	}

	_rt.init(buf, size, cb);
	int ret = _read(buf, size);
	if (ret == 0)    // 접속 종료
	{
#ifdef DEBUG
		LOG_DEBUG("readAll 2 [%d]", _soc);
#endif
		_rt.set(0, Token::DONE);    // read 0 후 끊어진다
		_proactor->_enqueue(&_rt);
	}
	else if (ret < 0)
	{
		if (_rt.err.code == EAGAIN)
		{
#ifdef DEBUG
			LOG_DEBUG("readAll 3 [%d]", _soc);
#endif
			_rt.set(0, Token::ALL);    // onRead 에서 더 읽는다
		}
		else
		{
#ifdef DEBUG
			LOG_DEBUG("readAll 4 [%d]", _soc);
#endif
			_rt.set(0, Token::ERR);  // 컴플리션 핸들러에 _err 전달
			_proactor->_enqueue(&_rt);
		}
	}
	else    // 읽은게 있는 경우
	{
		if (ret < size)    // 읽은게 있지만 덜 읽은 경우 onRead 로 간다
		{
#ifdef DEBUG
			LOG_DEBUG("readAll 5 [%d]", _soc);
#endif
			_rt.set(ret, Token::ALL);    // onRead 에서 더 읽는다
		}
		else    // 다 읽은 경우
		{
#ifdef DEBUG
			LOG_DEBUG("readAll 6 [%d]", _soc);
#endif
			_rt.set(ret, Token::DONE);
			_proactor->_enqueue(&_rt);
		}
	}
	return true;
}

// bool Socket::readvAll(const IoVec& iovConst, T* t, void (T::*f)(Error&, int))
// {
// #ifdef DEBUG
// 	LOG_DEBUG("readvAll 1");
// #endif
// 	if (_readACT)
// 	{
// 		_err.set(-1, "_readACT is not null");
// 		return false;
// 	}
//
// 	IoVec iov(iovConst);    // 내부 값을 변화하므로 복사해서 사용
// 	int ret = _readv(iov);
// 	if (ret == 0)    // 접속 종료
// 	{
// #ifdef DEBUG
// 		LOG_DEBUG("readvAll 2");
// #endif
// 		_proactor->_enqueue(new IovACT<T> (ACT::IOV, t, f, 0));    // 접속 종료
// 	}
// 	else if (ret < 0)
// 	{
// 		if (_err.code == EAGAIN)    // onRead에서 _readACT 를 사용하여 더 읽는다
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("readvAll 3");
// #endif
// 			_readACT = new IovACT<T> (ACT::IOV, t, f, iov, 0);
// 		}
// 		else
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("readvAll 4 [%d][%s]", _err.code, _err.detail);
// #endif
// 			_proactor->_enqueue(new IovACT<T> (ACT::IOV, t, f, _err));
// 		}
// 	}
// 	else    // 읽은게 있는 경우
// 	{
// 		if (iov.complete() == false)    // 읽은게 있지만 덜 읽은 경우 onRead 로 간다
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("readvAll 5");
// #endif
// 			_readACT = new IovACT<T> (ACT::IOV, t, f, iov, ret);
// 		}
// 		else    // 다 읽은 경우
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("readvAll 6");
// #endif
// 			_proactor->_enqueue(new IovACT<T> (ACT::IOV, t, f, ret));
// 		}
// 	}
// 	return true;
// }

bool Socket::writeAll(char* buf, int size, Callback cb)
{
#ifdef DEBUG
	LOG_DEBUG("writeAll 1 [%d]", _soc);
#endif
	if (_wt.mode != Token::NONE)
	{
#ifdef DEBUG
		LOG_DEBUG("writeAll failed [%c]", _wt.mode);
#endif
		_wt.err.set(-1, "write in progress");
		return false;
	}

	_wt.init(buf, size, cb);
	int ret = _write(buf, size);
	if (ret <= 0)
	{
		if (_wt.err.code == EAGAIN)
		{
#ifdef DEBUG
			LOG_DEBUG("writeAll 2 [%d]", _soc);
#endif
			_wt.set(0, Token::ALL);    // onWrite 에서 더 읽는다
		}
		else
		{
#ifdef DEBUG
			LOG_DEBUG("writeAll 3 [%d]", _soc);
#endif
			_wt.set(0, Token::ERR);  // 컴플리션 핸들러에 _err 전달
			_proactor->_enqueue(&_wt);
		}
	}
	else    // write 한게 있는 경우
	{
		if (ret < size)    // write 한게 있지만 덜 write 한 경우 onWrite 로 간다
		{
#ifdef DEBUG
			LOG_DEBUG("writeAll 4 [%d]", _soc);
#endif
			_wt.set(ret, Token::ALL);    // onWrite 에서 더 읽는다
		}
		else    // 다 write 한 경우
		{
#ifdef DEBUG
			LOG_DEBUG("writeAll 5 [%d]", _soc);
#endif
			_wt.set(ret, Token::DONE);
			_proactor->_enqueue(&_wt);
		}
	}
	return true;
}

// bool Socket::writevAll(const IoVec& iovConst, Callback cb)
// {
// #ifdef DEBUG
// 	LOG_DEBUG("writevAll 1");
// #endif
// 	if (_writeACT)
// 	{
// 		_err.set(-1, "_writeACT is not null");
// 		return false;
// 	}
//
// 	IoVec iov(iovConst);    // 내부 값을 변화하므로 복사해서 사용
// 	int ret = _writev(iov);
// 	if (ret <= 0)
// 	{
// 		if (_err.code == EAGAIN)    // onWrite에서 _writeACT 를 사용하여 더 write
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("writevAll 2");
// #endif
// 			_writeACT = new IovACT<T> (ACT::IOV, t, f, iov, 0);
// 		}
// 		else
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("writevAll 3");
// #endif
// 			_proactor->_enqueue(new IovACT<T> (ACT::IOV, t, f, _err)); // 오류 전달
// 		}
// 	}
// 	else    // write 한게 있는 경우
// 	{
// 		if (iov.complete() == false)    // write 한게 있지만 덜 write 한 경우 onWrite 로 간다
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("writevAll 4");
// #endif
// 			_writeACT = new IovACT<T> (ACT::IOV, t, f, iov, ret);
// 		}
// 		else    // 다 write 한 경우
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("writevAll 5");
// #endif
// 			_proactor->_enqueue(new IoACT<T> (ACT::IOV, t, f, ret));
// 		}
// 	}
// 	return true;
// }

void Socket::close()
{
	_proactor->_remove(this);
	SAFE_CLOSE_SOCKET(_soc);
}

// bool Socket::_connect(const char* ip, unsigned short port)
// {
// 	struct sockaddr_in addr;
// 	MakeSockAddrIn(ip, port, &addr);
//
// 	_soc = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
// 	if (-1 == _soc)
// 	{
// 		_err.set(errno, "create socket fail [%d][%s]", errno, strerror(errno));
// 		return false;
// 	}
//
// 	if (SetNonBlock(_soc, true, &_err) == false)    // nonblocking
// 	{
// 		//_err.set(errno, "SetNonBlock fail [%d][%s]", errno, strerror(errno));
// 		return false;
// 	}
//
// 	if (::connect(_soc, (struct sockaddr*)&addr, sizeof(addr)) != 0)
// 	{
// 		if (errno != EINPROGRESS && errno != EAGAIN)
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG("connect error [%d][%s]", errno, strerror(errno));
// #endif
// 			_err.set(errno, "connect fail [%d][%s]", errno, strerror(errno));
// 			::close(_soc);
// 			return false;
// 		}
// #ifdef DEBUG
// 		else
// 		{
// 			LOG_DEBUG("connect error EINPROGRESS or EAGAIN [%d][%s]", errno, strerror(errno));
// 		}
// #endif
// 	}
// 	return true;
// }

int Socket::_read(char* buf, int size)
{
	_rt.err.clear();
	int ret = (int)::read(_soc, buf, size);
	if (ret < 0)
	{
		_rt.err.set(errno, "recv fail [%d][%s]", errno, strerror(errno));
	}
	return ret;


	/*_err.clear();

	int recvSize = 0;
	while (recvSize < size)
	{
		int ret = (int)::read(_soc, buf + recvSize, size - recvSize);
#ifdef DEBUG
		LOG_DEBUG(">>>>> read ret [%d]", ret);
#endif
		if (ret == 0)
		{
#ifdef DEBUG
			LOG_DEBUG(">>>>> read ZERO");
#endif
			break;
		}
		else if (ret < 0)
		{
			if (errno == EAGAIN)
			{
				if (recvSize > 0)
					break;    // EAGAIN 이고 recvSize > 0 이면 recv 성공이므로 recvSize 리턴
				else
					_err.code = errno;    // EAGAIN 만 설정
			}
			else
			{
				_err.set(errno, "recv fail [%d][%s]", errno, strerror(errno));
			}
			return ret;
		}
		recvSize += ret;
	}
	return recvSize;*/
}

int Socket::_write(const char* buf, int size)
{
	_wt.err.clear();
	int ret = (int)::write(_soc, buf, size);
	if (ret < 0)
	{
		_wt.err.set(errno, "send fail [%d][%s]", errno, strerror(errno));
	}
	return ret;


	/*_err.clear();

	int sendSize = 0;
	while (sendSize < size)
	{
		int ret = (int)::write(_soc, buf + sendSize, size - sendSize);
		if (ret <= 0)
		{
			if (errno == EAGAIN)
			{
				if (sendSize > 0)
					break;    // EAGAIN 이고 sendSize > 0 이면 send 성공이므로 sendSize 리턴
				else
					_err.code = errno;
			}
			else
				_err.set(errno, "send fail [%d][%s]", errno, strerror(errno));
			return ret;
		}
		sendSize += ret;
	}
	return sendSize;*/
}

// int Socket::_readv(IoVec& iov)
// {
// 	_err.clear();
//
// 	int recvSize = 0;
// 	while (iov.complete() == false)
// 	{
// #ifdef DEBUG
// 		LOG_DEBUG(">>>>> readv iov [_idx:%d][count:%d][iov_len:%zu]", iov._idx, iov.count(), iov.iov()->iov_len);
// #endif
// 		int ret = (int)::readv(_soc, iov.iov(), iov.count());
// #ifdef DEBUG
// 		LOG_DEBUG(">>>>> readv ret [%d]", ret);
// #endif
// 		if (ret == 0)
// 		{
// #ifdef DEBUG
// 			LOG_DEBUG(">>>>> readv ZERO");
// #endif
// 			break;
// 		}
// 		else if (ret < 0)
// 		{
// 			if (errno == EAGAIN)
// 			{
// 				if (recvSize > 0)
// 					break;    // EAGAIN 이고 recvSize > 0 이면 recv 성공이므로 recvSize 리턴
// 				else
// 					_err.code = errno;    // EAGAIN 만 설정
// 			}
// 			else
// 				_err.set(errno, "recv fail [%d][%s]", errno, strerror(errno));
// 			return ret;
// 		}
// 		iov.addIoSize(ret);
// 		recvSize += ret;
// 	}
// 	return recvSize;
// }

// int Socket::_writev(IoVec& iov)
// {
// 	_err.clear();
//
// 	int sendSize = 0;
// 	while (iov.complete() == false)
// 	{
// 		int ret = (int)::writev(_soc, iov.iov(), iov.count());
// 		if (ret <= 0)
// 		{
// 			if (errno == EAGAIN)
// 			{
// 				if (sendSize > 0)
// 					break;    // EAGAIN 이고 sendSize > 0 이면 send 성공이므로 sendSize 리턴
// 				else
// 					_err.code = errno;
// 			}
// 			else
// 				_err.set(errno, "send fail [%d][%s]", errno, strerror(errno));
// 			return ret;
// 		}
// 		iov.addIoSize(ret);
// 		sendSize += ret;
// 	}
// 	return sendSize;
// }

// user 가 delete this 하지 않은 경우
// read / write 완료이므로 release
// 다음 read / write 를 위해서 NULL
/*#ifndef DEBUG
#define CHECK_END(act) \
	do{ if (act->_refCnt == 2) \
	{ \
		act->release(); \
		act = NULL; \
	} } while(0)
#else
// 이것은 디버깅 용
#define CHECK_END(act) \
	LOG_DEBUG("CHECK_END start"); \
	do{ if (act->_refCnt == 2) \
	{ \
		LOG_DEBUG("release act and assign NULL"); \
		act->release(); \
		act = NULL; \
	} } while(0)
#endif*/

void Socket::onRead()
{
#ifdef DEBUG
	LOG_DEBUG("Socket::onRead [%d]", _soc);
#endif
	if (_rt.mode == Token::SOME)
	{
#ifdef DEBUG
		LOG_DEBUG("Socket::onRead 1 [%d]", _soc);
#endif
		int ret = _read(_rt.buf, _rt.bufSize);
		if (ret == 0)    // 접속 종료
		{
#ifdef DEBUG
			LOG_DEBUG("Socket::onRead 2 [%d]", _soc);
#endif
			_rt.set(0, Token::DONE);    // read 0 후 끊어진다
			_proactor->_enqueue(&_rt);
			return;
		}
		else if (ret < 0)
		{
			if (_rt.err.code == EAGAIN)
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onRead 3 [%d]", _soc);
#endif
				// 사실 event 발생시 이 경우는 발생하면 안된다
				// 아무것도 하지 않고 다음 event 를 대기
				return;
			}
			else    // error
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onRead 4 [%d]", _soc);
#endif
				_rt.set(0, Token::ERR);    // _err 전달
				_proactor->_enqueue(&_rt);
				return;
			}
		}
		else
		{
#ifdef DEBUG
			LOG_DEBUG("Socket::onRead 5 [%d]", _soc);
#endif
			_rt.set(ret, Token::DONE);    // 읽은 양 전달
			_proactor->_enqueue(&_rt);
			return;
		}
	}
	else if (_rt.mode == Token::ALL)
	{
#ifdef DEBUG
		LOG_DEBUG("Socket::onRead 6 [%d]", _soc);
#endif
		int ret = _read(_rt.buf + _rt.ioSize, _rt.bufSize - _rt.ioSize);
		if (ret == 0)    // 접속 종료
		{
#ifdef DEBUG
			LOG_DEBUG("Socket::onRead 7 [%d]", _soc);
#endif
			_rt.set(0, Token::DONE);    // read 0 후 끊어진다
			_proactor->_enqueue(&_rt);
			return;
		}
		else if (ret < 0)
		{
			if (_rt.err.code == EAGAIN)
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onRead 8 [%d]", _soc);
#endif
				// 사실 event 발생시 이 경우는 발생하면 안된다
				// 아무것도 하지 않고 다음 event 를 대기
				return;
			}
			else    // error
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onRead 9 [%d]", _soc);
#endif
				_rt.set(0, Token::ERR);    // _err 전달
				_proactor->_enqueue(&_rt);
				return;
			}
		}
		else
		{
			_rt.add(ret);
			if (_rt.ioSize < _rt.bufSize)
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onRead 10 [%d]", _soc);
#endif
				// 아무것도 하지 않고 다음 event 를 대기
				return;
			}
			else
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onRead 11 [%d]", _soc);
#endif
				_rt.set(_rt.ioSize, Token::DONE);
				_proactor->_enqueue(&_rt);
				return;
			}
		}
	}
}

void Socket::onWrite()
{
#ifdef DEBUG
	LOG_DEBUG("Socket::onWrite [%d]", _soc);
#endif

	if (_wt.mode == Token::ALL)
	{
#ifdef DEBUG
		LOG_DEBUG("Socket::onWrite 1 [%d]", _soc);
#endif
		int ret = _write(_wt.buf + _wt.ioSize, _wt.bufSize - _wt.ioSize);
		if (ret <= 0)
		{
			if (_wt.err.code == EAGAIN)
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onWrite 1 [%d]", _soc);
#endif
				// 사실 event 발생시 이 경우는 발생하면 안된다
				// 아무것도 하지 않고 다음 event 를 대기
				return;
			}
			else    // error
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onWrite 2 [%d]", _soc);
#endif
				_wt.set(0, Token::ERR);    // _err 전달
				_proactor->_enqueue(&_wt);
				return;
			}
		}
		else
		{
			_wt.add(ret);
			if (_wt.ioSize < _wt.bufSize)
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onWrite 3 [%d]", _soc);
#endif
				// 아무것도 하지 않고 다음 event 를 대기
				return;
			}
			else
			{
#ifdef DEBUG
				LOG_DEBUG("Socket::onWrite 4 [%d]", _soc);
#endif
				_wt.set(_wt.ioSize, Token::DONE);
				_proactor->_enqueue(&_wt);
				return;
			}
		}
	}
}

void Socket::onClose()
{
#ifdef DEBUG
	LOG_DEBUG("Socket::onClose");
#endif
	_rt.err.set(ECONNRESET, "closed");
	_wt.err.set(ECONNRESET, "closed");
	if (_rt.mode != Token::DONE)
		_rt.cb(_rt.err, 0);
	else if (_wt.mode != Token::DONE)
		_wt.cb(_wt.err, 0);
}

Token::Token() : buf(NULL), bufSize(0), ioSize(0), mode(NONE), prev(NULL),
	next(NULL)
{
}

void Token::init(char* buf, int bufSize, Callback cb)
{
	this->buf = buf;
	this->bufSize = bufSize;
	this->ioSize = 0;
	this->cb = cb;
	this->mode = NONE;
}

void Token::set(int ioSize, char mode)
{
	this->ioSize = ioSize;
	this->mode = mode;
}

void Token::add(int ioSize)
{
	this->ioSize += ioSize;    // add
}

Acceptor::Acceptor(Proactor* proactor) : _soc(INVALID_SOCKET), _proactor(proactor)
{
}

Acceptor::Acceptor(SOCKET soc, Proactor* proactor) : _soc(soc), _proactor(proactor)
{
}

Acceptor::~Acceptor()
{
	if (_soc != INVALID_SOCKET)
		close();
}

bool Acceptor::listen(const char* ip, unsigned short port, int backlog, 
		AcceptCallback cb)
{
	if (_listen(ip, port, backlog) == false)
		return false;
	_cb = cb;
	return _proactor->_register(this, true);
}

void Acceptor::close()
{
	_proactor->_remove(this);
	SAFE_CLOSE_SOCKET(_soc);
#ifdef DEBUG
	LOG_DEBUG("Acceptor::close");
#endif
}

bool Acceptor::_listen(const char* ip, unsigned short port, int backlog)
{
	struct sockaddr_in addr;
	MakeSockAddrIn(ip, port, &addr);

	_soc = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (-1 == _soc)
	{   
		_err.set(errno, "create socket fail [%d][%s]", errno, strerror(errno));
		return false;
	}

	int option = 0;
	if (::setsockopt(_soc, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) != 0)
	{   
		_err.set(errno, "setsockopt fail [%d][%s]", errno, strerror(errno));
		return false;
	}

	if (SetNonBlock(_soc, true, &_err) == false)    // nonblocking
	{
		//_err.set(errno, "SetNonBlock fail [%d][%s]", errno, strerror(errno));
		return false;
	}

	if (::bind(_soc, (struct sockaddr*)&addr, sizeof(addr)) != 0)
	{   
		_err.set(errno, "bind fail [%d][%s]", errno, strerror(errno));
		return false;
	}

	if (::listen(_soc, backlog) != 0)
	{
		_err.set(errno, "listen fail [%d][%s]", errno, strerror(errno));
		return false;
	}

	return true;
}

SOCKET Acceptor::_accept(char* clientIp)
{
	_err.clear();

	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);

	int soc = ::accept(_soc, (struct sockaddr*)&addr, &len);    // nonblocking accept
	if (-1 == soc)
	{
		if (errno == EAGAIN)
			_err.code = errno;
		else
			_err.set(errno, "accept fail [%d][%s]", errno, strerror(errno));
		return -1;
	}

	if (SetNonBlock(soc, true, &_err) == false)    // nonblocking client
	{
		//_err.set(errno, "SetNonBlock fail [%d][%s]", errno, strerror(errno));
		return -1;
	}
	if (clientIp != NULL)
	{
		if (Addr2str(&addr, clientIp) == false)
		{
			_err.set(errno, "Addr2str fail [%d][%s]", errno, strerror(errno));
			return -1;
		}
	}
	return soc;
}

void Acceptor::onRead()
{
#ifdef DEBUG
	LOG_DEBUG("Acceptor::onRead");
#endif
	while (true)
	{
		SOCKET soc = _accept(NULL);
#ifdef DEBUG
		LOG_DEBUG("Acceptor::_accept return [%d][%d][%s]", soc, _err.code, _err.detail);
#endif
		if (soc < 0)    // error
		{
			if (_err.code == EAGAIN)
			{
#ifdef DEBUG
				LOG_DEBUG("Acceptor::_accept error 1 [%d][%d][%s]", soc, _err.code, _err.detail);
#endif
				return;
			}
			else
			{
#ifdef DEBUG
				LOG_DEBUG("Acceptor::_accept error 2 [%d][%d][%s]", soc, _err.code, _err.detail);
#endif
				_cb(_err, soc);    // soc : -1
				return;
			}
		}
		else
		{
#ifdef DEBUG
			LOG_DEBUG("Acceptor::_accept callback [%d][%d][%s]", soc, _err.code, _err.detail);
#endif
			_err.clear();
			_cb(_err, soc);
		}
	}
}

void Acceptor::onWrite()
{
}

void Acceptor::onClose()
{
#ifdef DEBUG
	LOG_DEBUG("Acceptor::onClose");
#endif
	_err.set(ECONNRESET, "closed");
	_cb(_err, INVALID_SOCKET);
}

Proactor::Proactor() : _fd(-1), _stop(false)
{
	_err.clear();
	// _head.clear();
	_head.next = &_head;
	_head.prev = &_head;
}

void Proactor::stop()
{
	_stop = true;
}

void Proactor::_enqueue(Token* token)
{
	token->next = &_head;
	token->prev = _head.prev;
	_head.prev->next = token;
	_head.prev = token;
}

void Proactor::_remove(Token* token)
{
	token->next->prev = token->prev;
	token->prev->next = token->next;
	token->next = token->prev = NULL;
}

void Proactor::_processCompletion()
{
	while (_head.next != &_head)
	{
		Token* cur = _head.next;    // 작업 대상
		_remove(cur);        // 빼버린다
		cur->mode = Token::NONE;
		cur->cb(cur->err, cur->ioSize);
	}

	// Token* cur = _head.next;
	// while (cur != &_head)
	// {
	// 	Token* temp = cur;    // 작업 대상
	// 	cur = cur->next;      // 미리 이동한다
	// 	_remove(temp);        // 빼버린다
    //
	// 	temp->mode = Token::NONE;
	// 	temp->cb(temp->err, temp->ioSize);
	// }
}

}    // end of namespace aio
