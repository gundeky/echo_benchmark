#include "proactor.h"
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/epoll.h>

namespace aio
{

bool Proactor::create(int pollIntervalMsec)
{
	_pollIntervalMsec = pollIntervalMsec;
	_fd = ::epoll_create(MAX_HANDLE); 
	if (_fd == -1)
	{
		_err.set(errno, "epoll_create fail [%d][%s]", errno, strerror(errno));
		return false;
	}
	return true;
}

bool Proactor::run()
{
	struct epoll_event events[MAX_HANDLE];
	while(1)
	{
// #ifdef DEBUG
// 		LOG_DEBUG("proactor enter epoll");
// #endif

		int ret = ::epoll_wait(_fd, events, MAX_HANDLE, _pollIntervalMsec);
		if (_stop == true)
		{
#ifdef DEBUG
			LOG_DEBUG("proactor stopped\n");
#endif
			SAFE_CLOSE_HANDLE(_fd);
			return true;
		}
		if (ret < 0)    // error
		{
			_err.set(errno, "epoll_wait fail [%d][%s]", errno, strerror(errno));
#ifdef DEBUG
			LOG_DEBUG("proactor error [%d][%s]\n", _err.code, _err.detail);
#endif
			SAFE_CLOSE_HANDLE(_fd);
			return false;
		}
		else if (ret == 0)    // timeout
		{
			_processTimer();    // 타이머만 확인하고 나간다
			continue;
		}

		for (int i=0; i<ret; ++i)
		{
#ifdef DEBUG
			LOG_DEBUG("---------- enter loop ----------");
#endif
			struct epoll_event& ev = events[i];
			EventHandler* handler = reinterpret_cast<EventHandler*>(ev.data.ptr);

			if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP))
			{
#ifdef DEBUG
				LOG_DEBUG("handler->onClose [%d][%d][%p]", i, handler->handle(), handler);
#endif
				handler->onClose();
				continue;
			}

			if ((ev.events & EPOLLIN) || (ev.events & EPOLLPRI))
			{
#ifdef DEBUG
				LOG_DEBUG("handler->onRead [%d][%d][%p]", i, handler->handle(), handler);
#endif
				_deleted.clear();
				handler->onRead();
				// 매 핸들러에 대해서 발생한 모든 컴플리션을 처리한다
				_processCompletion();
				// 만약 handler 가 삭제되었다면 밑으로 진행하지 않는다
				if (std::find(_deleted.begin(), _deleted.end(), handler) != _deleted.end())
				{
#ifdef DEBUG
					LOG_DEBUG(">>>>> _deleted [%p]", handler);
#endif
					continue;
				}
			}

			if (ev.events & EPOLLOUT)
			{
#ifdef DEBUG
				LOG_DEBUG("handler->onWrite [%d][%d][%p]", i, handler->handle(), handler);
#endif
				handler->onWrite();
				// 매 핸들러에 대해서 발생한 모든 컴플리션을 처리한다
				_processCompletion();
			}
		}

		_processTimer();    // 다 끝나고 타이머 확인
	}
	return true;
}

bool Proactor::_register(EventHandler* handler, bool acceptor)
{
	if (handler->handle() == INVALID_SOCKET)
	{
		_err.set(-1, "INVALID_SOCKET [%d]");
		return false;
	}

	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	if (acceptor)
		ev.events = EPOLLIN | EPOLLET;    // edge trigger
	else
		ev.events = EPOLLIN | EPOLLOUT | EPOLLET;    // edge trigger
	ev.data.ptr = handler;

	if(::epoll_ctl(_fd, EPOLL_CTL_ADD, handler->handle(), &ev) == -1)
	{
		_err.set(errno, "epoll_ctl error [%d][%d][%s]", handler->handle(), errno, strerror(errno));
		return false;
	}
	return true;
}

bool Proactor::_remove(EventHandler* handler)
{
	_deleted.push_back(handler);
	if (::epoll_ctl(_fd, EPOLL_CTL_DEL, handler->handle(), NULL) == -1)
	{
		_err.set(errno, "epoll_ctl error [%d][%d][%s]", handler->handle(), errno, strerror(errno));
		return false;
	}
	return true;
}

}    // end of namespace aio
