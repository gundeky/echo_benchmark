#ifndef __CPL_LOG_H_E933B4CB_436E_4876_B720_CBA00E7A8A3D__
#define __CPL_LOG_H_E933B4CB_436E_4876_B720_CBA00E7A8A3D__

#ifdef WIN32

#include <windows.h>
#include <string>
#include <string.h>
#include <stdio.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE     8192
#endif

#ifndef MAX_FILENAME
#define MAX_FILENAME    256
#endif

namespace cpl
{

	class Logger
	{
	public:
		enum LOG_LEVEL
		{
			LEVEL_TRACE = 0,
			LEVEL_DEBUG = 1,
			LEVEL_INFO  = 2,
			LEVEL_ERROR = 3
		};

		Logger() : _hFile(NULL), _level(LEVEL_DEBUG), _rotateSize(0)
			, _enableLock(false), _enableTid(false), _daily(false), _hourly(false)
		{
			memset(&_prev, 0, sizeof(_prev));
			_prev.wYear   = (WORD)-1;
			_prev.wMonth  = (WORD)-1;
			_prev.wDay    = (WORD)-1;
			_prev.wHour   = (WORD)-1;
			_prev.wMinute = (WORD)-1;
			_prev.wSecond = (WORD)-1;
		}

		~Logger()
		{
			close();
		}

		bool init(const char* logFile, LOG_LEVEL level, size_t rotateSize = 0
			, bool enableLock = false, bool enableTid = false, bool daily = false, bool hourly = false)
		{
			if (_getAbsFilePath(logFile) == false)
			{
				fprintf(stderr, "_checkDirectory(%s) failed\r\n", logFile);
				return false;
			}

			_hFile = CreateFile(logFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (_hFile == INVALID_HANDLE_VALUE)
				return false;

			_level = level;
			_rotateSize = rotateSize;
			_enableLock = enableLock;
			_enableTid  = enableTid;
			_daily = daily;
			_hourly = hourly;

			if (_enableLock)
			{
				InitializeCriticalSection(&_cs);
			}

			return true;
		}

		void close()
		{
			if (_logFile.size() != 0 && _hFile != NULL)
			{
				CloseHandle(_hFile);
				_hFile = NULL;
				_logFile.clear();
			}

			if (_enableLock)
			{
				DeleteCriticalSection(&_cs);
			}
		}

		void reload(bool daily, bool hourly)
		{
			_daily = daily;
			_hourly = hourly;
		}

		void log(LOG_LEVEL level, const char* file, int line, const char* fmt, ...)
		{
			if (level < _level)
				return;

			if (_hFile == NULL)
			{
				fprintf(stderr, "Logger not initialized\r\n");
				return;
			}

			char buf[BUFFER_SIZE];
			SYSTEMTIME now;
			GetLocalTime(&now);

			DWORD myPid = GetCurrentProcessId();

			char levelStr[10];
			switch(level)
			{
			case LEVEL_TRACE: strcpy_s(levelStr, sizeof(levelStr), "TRC"); break;
			case LEVEL_DEBUG: strcpy_s(levelStr, sizeof(levelStr), "DBG"); break;
			case LEVEL_INFO:  strcpy_s(levelStr, sizeof(levelStr), "INF"); break;
			case LEVEL_ERROR: strcpy_s(levelStr, sizeof(levelStr), "ERR"); break;
			default:    strcpy_s(levelStr, sizeof(levelStr), "UKN"); break;
			}

			char tidBuf[20] = {0, };
			if (_enableTid)
			{
				_snprintf_s(tidBuf, sizeof(tidBuf), _TRUNCATE, "|%lu", GetCurrentThreadId());
			}

			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%lu%s|%s|%04d-%02d-%02d %02d:%02d:%02d| %s(%d): "
				, myPid, tidBuf, levelStr , now.wYear, now.wMonth, now.wDay, now.wHour
				, now.wMinute, now.wSecond, file, line);
			size_t bufLen = ::strlen(buf);    // if you can believe the return value of snprintf

			va_list args;
			va_start(args, fmt);
			//bufLen += 
			_vsnprintf_s(buf + bufLen, sizeof(buf) - bufLen - 1, _TRUNCATE, fmt, args);    // -1 for \n
			va_end(args);
			bufLen += ::strlen(buf + bufLen);    // if you can believe the return value of vsnprintf

			strcat_s(buf + bufLen, sizeof(buf) - bufLen, "\n");
			bufLen += 1;    // length including \n

			write(buf, bufLen, &now);
		}

		void write(const char* buf, size_t size, SYSTEMTIME* now = NULL)
		{
			if (_enableLock)
			{
				EnterCriticalSection(&_cs);
			}

			if (now == NULL)
			{
				SYSTEMTIME now2;
				GetLocalTime(&now2);
				_checkRotate(now2);
			}
			else
			{
				_checkRotate(*now);
			}

			DWORD written = 0;
			WriteFile(_hFile, buf, size, &written, NULL);

			if (_enableLock)
			{
				LeaveCriticalSection(&_cs);
			}
		}

		static Logger* instance()
		{
			static Logger logger;
			return &logger;
		}

	private:
		bool _getAbsFilePath(const char* logFile_)
		{
			char logFile[MAX_PATH];
			strcpy_s(logFile, sizeof(logFile), logFile_);
			size_t len = strlen(logFile);
			for (size_t i = 0; i < len; ++i)    // 모든 / 를 \\로 바꾼다
			{
				if (logFile[i] == '/')
					logFile[i] = '\\';
			}
			const char* eod = strrchr(logFile, '\\');
			char logDir[MAX_PATH] = { 0, };
			const char* filename = NULL;
			if (eod == NULL)
			{
				if (strcmp(logFile, ".") == 0 || strcmp(logFile, "..") == 0)
				{
					return false;
				}
				strcpy_s(logDir, sizeof(logDir), ".");
				filename = logFile;
			}
			else
			{
				memcpy(logDir, logFile, eod - logFile);
				logDir[eod - logFile] = 0;
				filename = eod + 1;
				if (strlen(filename) == 0 || strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
				{
					return false;
				}
			}

			char realDir[MAX_PATH] = { 0, };
			DWORD ret = GetFullPathName(logDir, sizeof(realDir), realDir, NULL);
			if (ret == 0)
			{
				return false;
			}

			ret = GetFileAttributes(realDir);
			if (ret != FILE_ATTRIBUTE_DIRECTORY)   // it is not a directory
			{
				std::string err = _getLastErrorAsString();
				return false;
			}

			_logFile = realDir;
			_logFile += "\\";
			_logFile += filename;
			return true;
		}

		void _checkRotate(SYSTEMTIME& now)
		{
			if (_logFile.size() == 0 || (_rotateSize == 0 && _daily == false && _hourly == false))
			{
				return;
			}

			if (_hourly)
			{
				if (_prev.wHour != (WORD)-1 && 
					(_prev.wYear != now.wYear || 
					_prev.wMonth != now.wMonth ||
					_prev.wDay != now.wDay ||
					_prev.wHour != now.wHour))
				{
					_rotate(_prev);
					_copyTm(now);
					return;
				}
			}
			else if (_daily)    // hourly has priority to daily
			{
				if (_prev.wDay != (WORD)-1 && 
					(_prev.wYear != now.wYear || 
					_prev.wMonth  != now.wMonth  ||
					_prev.wDay != now.wDay ))
				{
					_rotate(_prev);
					_copyTm(now);
					return;
				}
			}

			// if _rotateSize > 0
			DWORD fileSizeHigh = 0;
			DWORD fileSize = GetFileSize(_hFile, &fileSizeHigh);
			if (fileSize == INVALID_FILE_SIZE)
			{
				fprintf(stderr, "::stat failed [%s][%s]\n", _logFile.c_str(), _getLastErrorAsString().c_str());
				return;
			}

			if (fileSize > _rotateSize)
			{
				_rotate(now);
			}
			_copyTm(now);
		}

		void _rotate(SYSTEMTIME& now)
		{
			CloseHandle(_hFile);

			char newLogFile[MAX_FILENAME + 1];
			_snprintf_s(newLogFile, sizeof(newLogFile), _TRUNCATE, "%s.%04d%02d%02d.%02d%02d%02d", _logFile.c_str()
				, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);


			DWORD ret = GetFileAttributes(newLogFile);
			if (ret != INVALID_FILE_ATTRIBUTES)    // if new log file already exists
			{
				strcat_s(newLogFile, sizeof(newLogFile), ".1");    // add post fix to new log file name
			}

			MoveFile(_logFile.c_str(), newLogFile);

			_hFile = CreateFile(_logFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (_hFile == INVALID_HANDLE_VALUE)
			{
				fprintf(stderr, "::open failed [%s][%s]\n", _logFile.c_str(), _getLastErrorAsString().c_str());
			}
		}

		void _copyTm(SYSTEMTIME& now)
		{
			_prev.wYear = now.wYear;
			_prev.wMonth = now.wMonth;
			_prev.wDay = now.wDay;
			_prev.wHour = now.wHour;
			_prev.wMinute = now.wMinute;
			_prev.wSecond = now.wSecond;
		}

		std::string _getLastErrorAsString()
		{
			//Get the error message, if any.
			DWORD errorMessageID = ::GetLastError();
			if (errorMessageID == 0)
				return std::string(); //No error message has been recorded

			LPSTR messageBuffer = nullptr;
			size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

			std::string message(messageBuffer, size);

			//Free the buffer.
			LocalFree(messageBuffer);

			return message;
		}

	private:
		std::string _logFile;
		HANDLE _hFile;
		LOG_LEVEL _level;
		size_t _rotateSize;
		bool   _enableLock;
		bool   _enableTid;
		CRITICAL_SECTION _cs;
		bool _daily;
		bool _hourly;
		SYSTEMTIME _prev;
	};

}    // end of namespace

#else    // #ifdef WIN32

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <stdarg.h>

#include <time.h>
#include <sys/types.h>    // for getpid, open
#include <unistd.h>       // for getpid
#include <sys/stat.h>     // for open
#include <fcntl.h>        // for open
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <limits.h>       // for realpath
#include <stdlib.h>       // for realpath

namespace cpl
{

class Logger
{
	public:
		enum LOG_LEVEL
		{
			LEVEL_TRACE = 0,
			LEVEL_DEBUG = 1,
			LEVEL_INFO  = 2,
			LEVEL_ERROR = 3
		};

		enum CONSTANT
		{
			BUFFER_SIZE = 8192,
			MAX_FILENAME = 256
		};

		Logger() : _fd(STDOUT_FILENO), _level(LEVEL_DEBUG), _rotateSize(0), _logBufSize(BUFFER_SIZE)
				   , _enableLock(false), _enableTid(false), _daily(false), _hourly(false)
		{
			memset(&_prev, 0, sizeof(_prev));
			_prev.tm_year = -1;
			_prev.tm_mon  = -1;
			_prev.tm_mday = -1;
			_prev.tm_hour = -1;
			_prev.tm_min  = -1;
			_prev.tm_sec  = -1;
		}

		~Logger()
		{
			close();
		}

		bool init(const char* logFile, LOG_LEVEL level, size_t rotateSize = 0, size_t logBufSize = BUFFER_SIZE
				  , bool enableLock = false, bool enableTid = false, bool daily = false, bool hourly = false)
		{
			if (_getAbsFilePath(logFile) == false)
			{
				fprintf(stderr, "_checkDirectory(%s) failed\n", logFile);
				return false;
			}

			_fd = ::open(logFile, O_CREAT | O_WRONLY | O_APPEND, 0664);
			if (_fd < 0)
			{
				fprintf(stderr, "::open failed [%s][%d][%s]\n", logFile, errno, strerror(errno));
				return false;
			}

			return init(_fd, level, rotateSize, logBufSize, enableLock, daily, hourly);
		}

		bool init(int32_t fd, LOG_LEVEL level, size_t rotateSize = 0, size_t logBufSize = BUFFER_SIZE
				  , bool enableLock = false, bool enableTid = false, bool daily = false, bool hourly = false)
		{
			_fd = fd;
			_level = level;
			_rotateSize = rotateSize;
			_logBufSize = logBufSize;
			_enableLock = enableLock;
			_enableTid  = enableTid;
			_daily = daily;
			_hourly = hourly;

			if (_enableLock)
			{
				int32_t ret = pthread_mutex_init(&_mtx, NULL);
				if (ret != 0)
				{
					fprintf(stderr, "::pthread_mutex_init failed [%d][%s]\n", ret, strerror(ret));
					return false;
				}
			}

			return true;
		}

		void close()
		{
			if (_logFile.size() != 0 && _fd != -1)
			{
				::close(_fd);
				_fd = -1;
				_logFile.clear();
			}

			if (_enableLock)
			{
				int32_t ret = pthread_mutex_destroy(&_mtx);
				if (ret != 0)
				{
					fprintf(stderr, "::pthread_mutex_destroy failed [%d][%s]\n", ret, strerror(ret));
				}
			}
		}

		void reload(bool daily, bool hourly)
		{
			_daily = daily;
			_hourly = hourly;
		}

		void log(LOG_LEVEL level, const char* file, int32_t line, const char* fmt, ...)
		{
			if (level < _level)
				return;

			if (_fd < 0)
			{
				fprintf(stderr, "Logger not initialized [%d]\n", _fd);
				return;
			}

			char buf[_logBufSize];
			time_t nowTime;
			struct tm now;

			time(&nowTime);
			localtime_r(&nowTime, &now);

			pid_t myPid = getpid();

			char levelStr[10];
			switch(level)
			{
				case LEVEL_TRACE: ::strcpy(levelStr, "TRC"); break;
				case LEVEL_DEBUG: ::strcpy(levelStr, "DBG"); break;
				case LEVEL_INFO:  ::strcpy(levelStr, "INF"); break;
				case LEVEL_ERROR: ::strcpy(levelStr, "ERR"); break;
				default:    ::strcpy(levelStr, "UKN"); break;
			}

			char tidBuf[20] = {0, };
			if (_enableTid)
			{
				snprintf(tidBuf, sizeof(tidBuf), "|%lu", (unsigned long)pthread_self());
			}

			::snprintf(buf, sizeof(buf), "%d%s|%s|%04d-%02d-%02d %02d:%02d:%02d| %s(%d): "
					   , myPid, tidBuf, levelStr , now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour
					   , now.tm_min	, now.tm_sec, file, line);
			int32_t bufLen = ::strlen(buf);    // if you can believe the return value of snprintf

			va_list args;
			va_start(args, fmt);
			//bufLen += 
			::vsnprintf(buf + bufLen, sizeof(buf) - bufLen - 1, fmt, args);    // -1 for \n
			va_end(args);
			bufLen += ::strlen(buf + bufLen);    // if you can believe the return value of vsnprintf

			::strcat(buf + bufLen, "\n");
			bufLen += 1;    // length including \n

			write(buf, bufLen, &now);
		}

		void write(const char* buf, size_t size, struct tm* now = NULL)
		{
			if (_enableLock)
			{
				int32_t ret = pthread_mutex_lock(&_mtx);
				if (ret != 0)
				{
					fprintf(stderr, "::pthread_mutex_lock failed [%d][%s]\n", ret, strerror(ret));
				}
			}

			if (now == NULL)
			{
				time_t nowTime;
				struct tm now2;

				time(&nowTime);
				localtime_r(&nowTime, &now2);

				_checkRotate(now2);
			}
			else
			{
				_checkRotate(*now);
			}

			::write(_fd, buf, size);

			if (_enableLock)
			{
				int32_t ret = pthread_mutex_unlock(&_mtx);
				if (ret != 0)
				{
					fprintf(stderr, "::pthread_mutex_lock failed [%d][%s]\n", ret, strerror(ret));
				}
			}
		}

		static Logger* instance()
		{
			static Logger logger;
			return &logger;
		}

	private:
		bool _getAbsFilePath(const char* logFile)
		{
			const char* eod = strrchr(logFile, '/');
			char logDir[256];
			const char* filename = NULL;
			if (eod == NULL)
			{
				if (strcmp(logFile, ".") == 0 || strcmp(logFile, "..") == 0)
				{
					return false;
				}
				strcpy(logDir, ".");
				filename = logFile;
			}
			else
			{
				memcpy(logDir, logFile, eod - logFile);
				logDir[eod - logFile] = 0;
				filename = eod + 1;
				if (strlen(filename) == 0 || strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
				{
					return false;
				}
			}

			char realDir[256];
			if (realpath(logDir, realDir) == NULL)
			{
				return false;
			}

			struct stat buf;
			if (::stat(logDir, &buf) == -1)    // stat error
			{
				return false;
			}

			if (S_ISDIR(buf.st_mode) == 0)   // it is not a directory
			{
				return false;
			}

			_logFile = realDir;
			_logFile += "/";
			_logFile += filename;
			return true;
		}

		void _checkRotate(struct tm& now)
		{
			if (_logFile.size() == 0 || (_rotateSize == 0 && _daily == false && _hourly == false))
			{
				return;
			}

			if (_hourly)
			{
				if (_prev.tm_hour != -1 && 
						(_prev.tm_year != now.tm_year || 
						 _prev.tm_mon  != now.tm_mon  ||
						 _prev.tm_mday != now.tm_mday ||
						 _prev.tm_hour != now.tm_hour))
				{
					_rotate(_prev);
					_copyTm(now);
					return;
				}
			}
			else if (_daily)    // hourly has priority to daily
			{
				if (_prev.tm_mday != -1 && 
						(_prev.tm_year != now.tm_year || 
						 _prev.tm_mon  != now.tm_mon  ||
						 _prev.tm_mday != now.tm_mday ))
				{
					_rotate(_prev);
					_copyTm(now);
					return;
				}
			}

			// if _rotateSize > 0
			struct stat buf;
			if (::stat(_logFile.c_str(), &buf) == -1)    // stat error
			{
				fprintf(stderr, "::stat failed [%s][%d][%s]\n", _logFile.c_str(), errno, strerror(errno));
				return;
			}

			if (buf.st_size > (off_t)_rotateSize)
			{
				_rotate(now);
			}
			_copyTm(now);
		}

		void _rotate(struct tm& now)
		{
			::close(_fd);

			char newLogFile[MAX_FILENAME + 1];
			::snprintf(newLogFile, sizeof(newLogFile), "%s.%04d%02d%02d.%02d%02d%02d", _logFile.c_str()
					, now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);

			struct stat buf;
			if (::stat(newLogFile, &buf) == 0)    // if new log file already exists
			{
				strcat(newLogFile, ".1");    // add post fix to new log file name
			}

			::rename(_logFile.c_str(), newLogFile);

			_fd = ::open(_logFile.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0664);
			if (_fd < 0)
			{
				fprintf(stderr, "::open failed [%s][%d][%s]\n", _logFile.c_str(), errno, strerror(errno));
			}
		}

		void _copyTm(struct tm& now)
		{
			_prev.tm_year = now.tm_year;
			_prev.tm_mon  = now.tm_mon;
			_prev.tm_mday = now.tm_mday;
			_prev.tm_hour = now.tm_hour;
			_prev.tm_min  = now.tm_min;
			_prev.tm_sec  = now.tm_sec;
		}

	private:
		std::string _logFile;
		int32_t _fd;
		LOG_LEVEL _level;
		size_t _rotateSize;
		size_t _logBufSize;
		bool   _enableLock;
		bool   _enableTid;
		pthread_mutex_t _mtx;
		bool _daily;
		bool _hourly;
		struct tm _prev;
};

}    // end of namespace

#endif    // #ifdef WIN32

#define LOG_INIT          cpl::Logger::instance()->init
#define LOG_TRACE(...)    cpl::Logger::instance()->log(cpl::Logger::LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...)    cpl::Logger::instance()->log(cpl::Logger::LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)     cpl::Logger::instance()->log(cpl::Logger::LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)    cpl::Logger::instance()->log(cpl::Logger::LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)






#if 0    // simple function log sample
#define LOG_INFO(...)     Log("INF", __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)    Log("ERR", __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...)    Log("DBG", __FILE__, __LINE__, __VA_ARGS__)

void Log(const char* lv, const char* file, int32_t line, const char* fmt, ...)
{
	char buf[8192];

	time_t nowTime = time(NULL);
	struct tm now = *localtime(&nowTime);

	snprintf(buf, sizeof(buf), "%s|%04d-%02d-%02d %02d:%02d:%02d| %s(%d): "
			, lv , now.tm_year+1900, now.tm_mon+1, now.tm_mday, now.tm_hour, now.tm_min
			, now.tm_sec, file, line);
	int32_t bufLen = strlen(buf);

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf + bufLen, sizeof(buf) - bufLen - 1, fmt, args);    // -1 for \n
	va_end(args);
	bufLen += strlen(buf + bufLen);

	::strcat(buf + bufLen, "\n");
	bufLen += 1;    // length including \n

	::write(1, buf, bufLen);
}


// log for windows
#include <time.h>
#include <windows.h>      // for GetCurrentProcessId
#include <stdint.h>

namespace cpl
{

#define MAX_LOG_BUF_SIZE    8192
#define MAX_FILENAME        256

/*!
  * @brief logger class
  */
class Logger
{
	public:
		enum LEVEL { LEVEL_DEBUG = 0, LEVEL_INFO = 1, LEVEL_ERROR = 2 };

		void init(const char* logFile, LEVEL level)    // log file name format : filename_YYYYMMDD
		{
			_logFile = logFile;
			_level = level;
		}

		void log(LEVEL level, const char* file, int32_t line, const char* fmt, ...)
		{
			if(level < _level)
				return;

			char buf[MAX_LOG_BUF_SIZE];
			time_t nowTime;
			struct tm now;

			time(&nowTime);
			localtime_s(&now, &nowTime);    // TODO is it thread safe ?

			DWORD myPid = GetCurrentProcessId();

			char levelStr[10]; 
			switch(level)
			{
				case LEVEL_DEBUG: ::strcpy_s(levelStr, sizeof(levelStr), "DBG"); break;
				case LEVEL_INFO:  ::strcpy_s(levelStr, sizeof(levelStr), "INF"); break;
				case LEVEL_ERROR: ::strcpy_s(levelStr, sizeof(levelStr), "ERR"); break;
				default:    ::strcpy_s(levelStr, sizeof(levelStr), "???"); break;
			}

			const char* filename = strrchr(file, '\\');
			if (filename != NULL)
			{
				++filename;
			}
			else
			{
				filename = file;
			}

			::_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%u|%s|%04d-%02d-%02d %02d:%02d:%02d| %s(%d): "
					, myPid, levelStr , now.tm_year+1900, now.tm_mon+1, now.tm_mday, now.tm_hour
					, now.tm_min, now.tm_sec, filename, line);
			int32_t bufLen = ::strlen(buf);

			va_list args;
			va_start(args, fmt);
			::vsnprintf_s(buf + bufLen, sizeof(buf) - bufLen - 1, _TRUNCATE, fmt, args);    // -1 for \n
			va_end(args);
			bufLen += ::strlen(buf + bufLen);

			::strcat_s(buf + bufLen, sizeof(buf) - bufLen, "\n");
			bufLen += 1;    // length including \n

			buf[bufLen] = 0;

			if(_logFile.size() == 0)
			{
				printf(buf);
				fflush(stdout);
			}
			else
			{
				char logFile[MAX_FILENAME + 1];
				::_snprintf_s(logFile, sizeof(logFile), _TRUNCATE, "%s_%04d%02d%02d", _logFile.c_str()
					, now.tm_year+1900, now.tm_mon+1, now.tm_mday);

				HANDLE file = CreateFile(logFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE
					, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if(file != INVALID_HANDLE_VALUE)
				{
					LARGE_INTEGER li;
					li.QuadPart = 0;
					if(::SetFilePointerEx(file, li, NULL, FILE_END) == TRUE)    // move to end of file
					{
						DWORD bytesWritten;
						WriteFile(file, buf, bufLen, &bytesWritten, NULL);
					}
					CloseHandle(file);
				}
				else
				{
					printf(buf);
					fflush(stdout);
				}
			}
		}

	private:
		std::string _logFile;
		LEVEL _level;
};

}    // end of namespace cpl

#endif    // for 0

#endif /*__CPL_LOG_H_E933B4CB_436E_4876_B720_CBA00E7A8A3D__*/
