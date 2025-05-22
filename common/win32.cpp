
#include "config.h"
#include "trace.h"

#include <errno.h>
#include <time.h>
#include <windows.h>

///////////////////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		// converting file time to unix epoch
		tmpres -= DELTA_EPOCH_IN_MICROSECS; 
		tmpres /= 10;  // convert into microseconds
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}

//---------------------------------------------------------------------------------------------

unsigned int sleep(unsigned int seconds)
{
	Sleep( (DWORD) seconds * 1000L);
	return seconds;
}


//---------------------------------------------------------------------------------------------

int inet_aton(const char *cp, struct in_addr *ina)
{
	unsigned long n;
	int rc;
	char *s;
	n = inet_addr(cp);
	rc = n != INADDR_NONE;
	*ina = *(struct in_addr *) &n;
	s = inet_ntoa(*ina);
	return rc;
}

//---------------------------------------------------------------------------------------------

int translate_wsaerror()
{
	int err = WSAGetLastError();
	switch (err)
	{
	case 0: 
		return 1;

//	case WSAENETDOWN: errno = ENETDOWN; break;
	case WSAEACCES: errno = EACCES; break;
	case WSAEINTR: errno = EINTR; break;
//	case WSAEINPROGRESS: errno = EINPROGRESS; break;
	case WSAEFAULT: errno = EFAULT; break;
//	case WSAENETRESET: errno = ENETRESET; break;
//	case WSAENOBUFS: errno = ENOBUFS; break;
//	case WSAENOTCONN: errno = ENOTCONN; break;
//	case WSAENOTSOCK: errno = ENOTSOCK; break;
//	case WSAEOPNOTSUPP: errno = EOPNOTSUPP; break;
//	case WSAESHUTDOWN: errno = ESHUTDOWN; break;
//	case WSAEWOULDBLOCK: errno = EWOULDBLOCK; break;
//	case WSAEMSGSIZE: errno = EMSGSIZE; break;
//	case WSAEHOSTUNREACH: errno = EHOSTUNREACH; break;
	case WSAEINVAL: errno = EINVAL; break;
//	case WSAECONNABORTED: errno = ECONNABORTED; break;
//	case WSAECONNRESET: errno = ECONNRESET; break;
//	case WSAETIMEDOUT: errno = ETIMEDOUT; break;

	default:
		return 0;
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////

CRITICAL_SECTION log_crit_sec;

//---------------------------------------------------------------------------------------------

static void dcc_win32_cleanup()
{
	WSACleanup();

	DeleteCriticalSection(&log_crit_sec);
}

//---------------------------------------------------------------------------------------------

void dcc_win32_startup()
{
	WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

	InitializeCriticalSection(&log_crit_sec);

	srand((unsigned int) time(0));

	atexit(dcc_win32_cleanup);
}

///////////////////////////////////////////////////////////////////////////////////////////////
