
#ifndef _rvfc_sync_atomic_
#define _rvfc_sync_atomic_

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4786 4348)
#endif

//#include "rvfc/exceptions/defs.h"
#include "rvfc/lang/general.h"

#include <stdio.h>

#ifdef _WIN32
#include "rvfc/win.h"
#endif

namespace rvfc
{

/////////////////////////////////////////////////////////////////////////////////////////////// 

#ifndef _WIN32
typedef unsigned long UINT32;
#endif

//---------------------------------------------------------------------------------------------

class AtomicUINT32
{
#if defined(__linux__)
	volatile long __attribute__ ((aligned (32))) _n; 

#elif defined(_WIN32)
	volatile __declspec(align(32)) LONG _n; 
#endif

public:
	AtomicUINT32(UINT32 k = 0) : _n(k) {}

	operator UINT32() const { return _n; }

	UINT32 operator++();
	UINT32 operator++(int);
	UINT32 operator--();
	UINT32 operator--(int);
};

//---------------------------------------------------------------------------------------------

inline UINT32 AtomicUINT32::operator++()
{
#ifdef __linux__
	return __sync_add_and_fetch(&_n, 1);

#elif defined(_WIN32)
	return InterlockedIncrement(&_n); 
#endif
}

//---------------------------------------------------------------------------------------------

inline UINT32 AtomicUINT32::operator++(int)
{
#ifdef __linux__
	return __sync_fetch_and_add(&_n, 1);

#elif defined(_WIN32)
	UINT32 n = _n;
	InterlockedIncrement(&_n);
	return n;
#endif
}

//---------------------------------------------------------------------------------------------

inline UINT32 AtomicUINT32::operator--()
{
#ifdef __linux__
	return __sync_sub_and_fetch(&_n, 1);

#elif defined(_WIN32)
	return InterlockedDecrement(&_n); 
#endif
}

//---------------------------------------------------------------------------------------------

inline UINT32 AtomicUINT32::operator--(int)
{
#ifdef __linux__
	return __sync_fetch_and_sub(&_n, 1);

#elif defined(_WIN32)
	UINT32 n = _n;
	InterlockedDecrement(&_n);
	return n;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////// 

} // namespace rvfc

#endif // _rvfc_sync_atomic_
