
#include "handle.h"

#include <stdexcept>
#include <io.h>

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////

HandleX::HandleX(FILE *file)
{
	_h = file ? (HANDLE) _get_osfhandle(_fileno(file)) : INVALID_HANDLE_VALUE;
}

//---------------------------------------------------------------------------------------------

void HandleX::close()
{
	if (_h != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_h);
		_h = INVALID_HANDLE_VALUE;
	}
}

//---------------------------------------------------------------------------------------------

Handle HandleX::dup(HandleX sourceProc, HandleX targetProc) const
{
	if (!isValid())
		throw std::runtime_error("cannot duplicate an invalid handle");
	return dupx(sourceProc, targetProc);
}

//---------------------------------------------------------------------------------------------

HandleX HandleX::dupx(HandleX sourceProc, HandleX targetProc) const
{
	if (!isValid())
		return INVALID_HANDLE_VALUE;

	HANDLE h;
	BOOL rc = DuplicateHandle(sourceProc, _h, targetProc, &h,
		0, TRUE, DUPLICATE_SAME_ACCESS);
	if (rc == FALSE)
		throw std::runtime_error("DuplicateHandle failed");
	return h;
}

//---------------------------------------------------------------------------------------------

void HandleX::setInheritable(bool b)
{
	if (SetHandleInformation(_h, HANDLE_FLAG_INHERIT, b ? 1 : 0) == FALSE)
		throw std::runtime_error("Handle: error setting inheritabiliy");
}

///////////////////////////////////////////////////////////////////////////////////////////////

void Handle::assertValid()
{
	if (!isValid())
		throw std::runtime_error("invalid handle");
}

//---------------------------------------------------------------------------------------------

HANDLE &Handle::toHandle()
{
	assertValid();
	return _h;
}

//---------------------------------------------------------------------------------------------

HANDLE Handle::toHandle() const
{
	return const_cast<Handle*>(this)->toHandle();
}

///////////////////////////////////////////////////////////////////////////////////////////////

StdHandle StdHandle::in  = StdHandle(STD_INPUT_HANDLE);
StdHandle StdHandle::out = StdHandle(STD_OUTPUT_HANDLE);
StdHandle StdHandle::err = StdHandle(STD_ERROR_HANDLE); 

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc
