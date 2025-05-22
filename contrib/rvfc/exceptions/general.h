
#ifndef _rvfc_exceptions_general_
#define _rvfc_exceptions_general_

#if defined(_WIN32)
#pragma warning(push)
#pragma warning(disable : 4786)
#if _MSC_VER > 1300
#pragma warning(disable : 4996)
#endif

#include "rvfc/win.h"

#elif defined(__linux__)
#include <errno.h>

#elif defined(__VXWORKS__)
#include <errnoLib.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdexcept>
#include <string>

#include "rvfc/lang/general.h"
#include "rvfc/text/defs.h"

namespace rvfc 
{

using std::string;

using namespace rvfc::Text;

///////////////////////////////////////////////////////////////////////////////////////////////

class Exception : public std::exception
{
public:
	class AbsOSError
	{
	public:
#if defined(__linux__)
		typedef int ErrorCode;
#elif defined(_WIN32)
		typedef DWORD ErrorCode;
#elif defined(__VXWORKS__)
		typedef int ErrorCode;
#endif

		bool hasInfo() const { return has_info; }
		ErrorCode code() const { return _code; }

		bool has_info;
		ErrorCode _code;
	};

	class NoOSError : public AbsOSError
	{
	public:
		NoOSError()
		{
			has_info = false;
			_code = 0;
		}
	};

	class OSError : public AbsOSError
	{
	public:
		OSError()
		{
			has_info = true;
#if defined(__linux__)
			_code = errno;
#elif defined(_WIN32)
			_code = GetLastError();
#elif defined(__VXWORKS__)
			_code = errnoGet();
#endif
		}
	};

	class SocketError : public AbsOSError
	{
	public:
		SocketError()
		{
			has_info = true;
#if defined(__linux__)
			_code = errno;
#elif defined(_WIN32)
			_code = WSAGetLastError();
#elif defined(__VXWORKS__)
			_code = errnoGet();
#endif
		}
	};

	//-----------------------------------------------------------------------------------------
public:
	AbsOSError os_err;
	string _file;
	int _line;
	mutable string _title, _report;

	void addDetails(const string &text) const;
	void addDetails(const char *fmt, ...) const;
	void addDetailsArgs(const char *fmt, va_list args) const;

	void addOSDetails() const;

	void addReport() const;
	void addReport(const Exception &x) const;

protected:
	Exception() {}

public:
	Exception(const char *file, int line, AbsOSError oserr, const char *title = "");
	virtual ~Exception() throw () {}

	const string &report() const throw();
	const char *what( ) const throw() { return report().c_str(); }

	AbsOSError::ErrorCode osErrorCode() const { return os_err.code(); }

	//-----------------------------------------------------------------------------------------
public:
	template <class T>
	class Adapter
	{
	public:
		typedef Adapter<T> This;

		T _t;

		Adapter(const T &t, AbsOSError oserr = Exception::NoOSError()) : _t(t) { _t.os_err = oserr; }

		This &operator()(const char *fmt, ...)
		{
			va_list args;
			va_start(args, fmt);
			_t.addDetailsArgs(fmt, args);
			va_end(args);
			return *this;
		}

		This &operator()(Exception &x)
		{
			_t.addReport(x);
			return *this;
		}

		This &operator()(Exception &x, const char *fmt, ...)
		{
			_t.addReport(x);

			va_list args;
			va_start(args, fmt);
			_t.addDetailsArgs(fmt, args);
			va_end(args);
			return *this;
		}

		operator const T&() const
		{                  
			_t.addReport();
			return _t;
		}
	};
};

///////////////////////////////////////////////////////////////////////////////////////////////

#define HERE __FILE__, __LINE__

//---------------------------------------------------------------------------------------------

#define THROW_EX(E) \
	throw (const E &) rvfc::Exception::Adapter<E>(E(__FILE__, __LINE__, \
		rvfc::Exception::NoOSError()))

#define THROW_OS(E) \
	throw (const E &) rvfc::Exception::Adapter<E>(E(__FILE__, __LINE__, \
		rvfc::Exception::OSError()))

#define THROW_SOCK(E) \
	throw (const E &) rvfc::Exception::Adapter<E>(E(__FILE__, __LINE__, \
		rvfc::Exception::SocketOSError()))

//---------------------------------------------------------------------------------------------

class LastException : public Exception
{
public:
	LastException(const Exception &x) : Exception(x) {}

	struct Proxy
	{
		Proxy &operator=(const Exception &x);
	};
};

//---------------------------------------------------------------------------------------------

#define ERROR_EX(E) \
	_invalidate(), rvfc::LastException::Proxy() = (const E &) rvfc::Exception::Adapter<E>(E(__FILE__, __LINE__, \
		rvfc::Exception::NoOSError()))

#define ERROR_OS(E) \
	_invalidate(), rvfc::LastException::Proxy() = (const E &) rvfc::Exception::Adapter<E>(E(__FILE__, __LINE__, \
		rvfc::Exception::OSError()))

#define ERROR_SOCK(E) \
	_invalidate(), rvfc::LastException::Proxy() = (const E &) rvfc::Exception::Adapter<E>(E(__FILE__, __LINE__, \
		rvfc::Exception::SocketOSError()))

void _invalidate();

///////////////////////////////////////////////////////////////////////////////////////////////

#define EXCEPTION_DEF(name, title) \
	class name : public rvfc::Exception \
	{ \
	public: \
		name(const char *file, int line, rvfc::Exception::AbsOSError oserr, const char *details = 0) : \
			rvfc::Exception(file, line, oserr, title) \
		{ if (details) addDetails("%s", details); } \
	}

//---------------------------------------------------------------------------------------------

#define SUB_EXCEPTION_DEF(name, super, title) \
	class name : public super \
	{ \
	public: \
		name(const char *file, int line, rvfc::Exception::AbsOSError oserr, const char *details = 0) : \
			super(file, line, oserr, title) \
		{ if (details) addDetails("%s", details); } \
	}

//---------------------------------------------------------------------------------------------

#define CLASS_EXCEPTION_DEF(title) \
	class Error : public rvfc::Exception \
	{ \
	public: \
		Error(const char *file, int line, rvfc::Exception::AbsOSError oserr, const char *details = 0) : \
			rvfc::Exception(file, line, oserr, title) \
		{ if (details) addDetails("%s", details); } \
	}

//---------------------------------------------------------------------------------------------

#define CLASS_SUB_EXCEPTION_DEF(name, title) \
	class name : public Error \
	{ \
	public: \
		name(const char *file, int line, rvfc::Exception::AbsOSError oserr, const char *details = 0) : \
			Error(file, line, oserr, title) \
		{ if (details) Error::addDetails("%s", details); } \
	}

//---------------------------------------------------------------------------------------------

EXCEPTION_DEF(Unimplemented, "Unimplemented feature");

//#define TOSSER throw (std::exception)
#define TOSSER throw (rvfc::Exception)

//---------------------------------------------------------------------------------------------

inline std::runtime_error error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	std::runtime_error x(vstringf(fmt, args));
	va_end(args);
	return x;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif // _rvfc_exceptions_general_
