
#if defined(_WIN32)
#include "rvfc/win.h"

#elif defined(__linux__)
#include <string.h>

#elif defined(__VXWORKS__)
#endif

#include "exceptions/general.h"

namespace rvfc
{

using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////

Exception::Exception(const char *file, int line, AbsOSError oserr, const char *title) :
	os_err(oserr),
	_title(title ? title : ""),
	_file(file),
	_line(line)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////

const string &Exception::report() const throw()
{
	try
	{
		if (_report.empty())
			addReport();
	}
	catch (...) {}
	return _report;
}

//---------------------------------------------------------------------------------------------

void Exception::addDetails(const string &text) const
{ 
	if (_title.empty())
		_title += text;
	else
		_title += ", " + text;
}

//---------------------------------------------------------------------------------------------

void Exception::addDetails(const char *fmt, ...) const
{
	if (!fmt)
		return;

	va_list args;
	va_start(args, fmt);
	addDetailsArgs(fmt, args);
	va_end(args);
}

//---------------------------------------------------------------------------------------------

void Exception::addDetailsArgs(const char *fmt, va_list args) const
{
	if (!fmt)
		return;

	char s[1024];
	vsprintf(s, fmt, args);
	addDetails(string(s));
}

//---------------------------------------------------------------------------------------------

void Exception::addOSDetails() const
{
#if defined(__linux__)
	char buf[256] = "";
	// it appears that strerror_r does not use buf (!)
	char *err = strerror_r(os_err.code(), buf, sizeof(buf));
	addDetails("os error: '%s' (%ld)", err, os_err);

#elif defined(_WIN32)
	char *msg;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, os_err.code(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &msg, 0, NULL);
	if (msg)
	{
		int m = strlen(msg);
		if (m > 1 && (msg[m-2] == '\r' || msg[m-2] == '\n'))
			msg[m-2]='\0';
		addDetails("os error: '%s' (%lu)", msg, os_err);
		LocalFree(msg);
	}
	else
		addDetails("os error: %lu", os_err);
#endif
}

//---------------------------------------------------------------------------------------------

void Exception::addReport() const
{
	if (os_err.hasInfo())
		addOSDetails();
	char s[1024];
#ifdef _DEBUG
	sprintf(s, "=> at (%s, %d): %s\n", _file.c_str(), _line, _title.empty() ? "exception" : _title.c_str());
#else
	sprintf(s, "=> %s\n", _title.empty() ? "exception" : _title.c_str());
#endif
	_report += s;
}

//---------------------------------------------------------------------------------------------

void Exception::addReport(const Exception &x) const
{
	_report += x.report();
}

///////////////////////////////////////////////////////////////////////////////////////////////

#if 1 // !defined(DEBUG) && !defined(_DEBUG)

//---------------------------------------------------------------------------------------------

__thread char __lastException_storage[sizeof(LastException)];
__thread LastException *__lastException = 0;

//---------------------------------------------------------------------------------------------

LastException::Proxy &LastException::Proxy::operator=(const Exception &x)
{
	if (!__lastException)
		__lastException = reinterpret_cast<LastException*>(&__lastException_storage);
	else
		__lastException->~LastException();
	new (__lastException) LastException(x);

#if defined(DEBUG) || defined(_DEBUG)
	printf("Exception: %s" , __lastException->what());
#endif

	return *this;
}

//---------------------------------------------------------------------------------------------

#endif // defined(DEBUG) || defined(_DEBUG)

//---------------------------------------------------------------------------------------------

void _invalidate()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc
