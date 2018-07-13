#include "proactor.h"
#include <unistd.h>
#include <string.h>
#include <sys/event.h>

namespace aio
{

bool Proactor::create(int pollIntervalMsec)
{
	_pollIntervalMsec = pollIntervalMsec;
	_fd = ::kqueue(); 
	if (_fd == -1)
	{
		_err.set(errno, "kqueue fail [%d][%s]", errno, strerror(errno));
		return false;
	}
	return true;
}

bool Proactor::run()
{
	struct kevent events[MAX_HANDLE];
	struct timespec timeout = {
		_pollIntervalMsec / 1000,
		(_pollIntervalMsec % 1000) * 1000000
	};
	while(1)
	{
#ifdef DEBUG
		LOG_DEBUG("proactor enter kqueue");
#endif

		int ret = ::kevent(_fd, NULL, 0, events, MAX_HANDLE, &timeout);
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
			_err.set(errno, "kevent fail [%d][%s]", errno, strerror(errno));
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
			struct kevent& ev = events[i];
			EventHandler* handler = reinterpret_cast<EventHandler*>(ev.udata);

			if (ev.flags & EV_EOF)
			{
#ifdef DEBUG
				LOG_DEBUG("handler->onClose [%d][%d][%p]", i, handler->handle(), handler);
#endif
				handler->onClose();
				continue;
			}

			if (ev.flags & EVFILT_READ)
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

			if (ev.flags & EVFILT_WRITE)
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

	struct kevent ev;
	if (acceptor)
		EV_SET(&ev, handler->handle(), EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, handler);
	else
		EV_SET(&ev, handler->handle(), EVFILT_READ | EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, handler);

	if (::kevent(_fd, &ev, 1, NULL, 0, NULL) == -1)
	{
		_err.set(errno, "kevent error [%d][%d][%s]", handler->handle(), errno, strerror(errno));
		return false;
	}
	return true;
}

bool Proactor::_remove(EventHandler* handler)
{
	_deleted.push_back(handler);
	struct kevent ev;
	EV_SET(&ev, handler->handle(), EVFILT_READ | EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	if (::kevent(_fd, &ev, 1, NULL, 0, NULL) == -1)
	{
		_err.set(errno, "kevent error [%d][%d][%s]", handler->handle(), errno, strerror(errno));
		return false;
	}
	return true;
}

}    // end of namespace aio
