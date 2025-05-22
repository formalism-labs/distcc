
#ifndef _rvfc_system_win32_sd_
#define _rvfc_system_win32_sd_

#include "rvfc/win.h"

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////

class SecurityDescriptor
{
	SECURITY_ATTRIBUTES _sd;
	BYTE _buf[SECURITY_DESCRIPTOR_MIN_LENGTH];

public:
	SecurityDescriptor()
	{
		if (!InitializeSecurityDescriptor((PSECURITY_DESCRIPTOR) &_sd, SECURITY_DESCRIPTOR_REVISION)) 
			throw std::runtime_error("error creating security descriptor");

		_sd.nLength = sizeof(_sd);
		_sd.lpSecurityDescriptor = (PSECURITY_DESCRIPTOR) &_buf;
		_sd.bInheritHandle = TRUE;
	}

	operator LPSECURITY_ATTRIBUTES() { return &_sd; }
	operator const SECURITY_ATTRIBUTES *() const { return &_sd; }
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_system_win32_sd_
