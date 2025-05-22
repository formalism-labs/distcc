
#ifndef _rvfc_system_win32_handle_
#define _rvfc_system_win32_handle_

#include "rvfc/win.h"

namespace rvfc
{

class Handle;

///////////////////////////////////////////////////////////////////////////////////////////////

class HandleX
{
protected:
	HANDLE _h;

public:
	HandleX(HANDLE h = INVALID_HANDLE_VALUE) : _h(h) {}
	HandleX(FILE *file);

	void close();

	bool isValid() const { return _h != INVALID_HANDLE_VALUE && _h != 0; }
	bool operator!() const { return !isValid(); }

	virtual HANDLE &toHandle() { return _h; }
	virtual HANDLE toHandle() const { return _h; }

	operator HANDLE() const { return toHandle(); }

	HANDLE operator*() const { return toHandle(); }
	HANDLE &operator*() { return toHandle(); }

	Handle dup(HandleX sourceProc = GetCurrentProcess(), HandleX targetProc = GetCurrentProcess()) const;
	HandleX dupx(HandleX sourceProc = GetCurrentProcess(), HandleX targetProc = GetCurrentProcess()) const;

	void setInheritable(bool b);
};

//---------------------------------------------------------------------------------------------

class Handle : public HandleX
{
protected:
	void assertValid();

public:
	Handle(const HandleX &h) : HandleX(h) { assertValid(); }
	Handle(HANDLE h) : HandleX(h) { assertValid(); }
	Handle(FILE *file) : HandleX(file) { assertValid(); }

	virtual HANDLE &toHandle();
	virtual HANDLE toHandle() const;
};

//---------------------------------------------------------------------------------------------

class StdHandle : public HandleX
{
public:
	StdHandle(DWORD id)
	{ 
		_h = GetStdHandle(id);
	}

	static StdHandle in;
	static StdHandle out;
	static StdHandle err;
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_system_win32_handle_
