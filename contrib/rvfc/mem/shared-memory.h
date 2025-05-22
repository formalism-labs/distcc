
#ifndef _rvfc_sync_shared_memory_
#define _rvfc_sync_shared_memory_

#include <stdexcept>

#ifdef _WIN32
#include "rvfc/win.h"
#include "rvfc/system/win32/handle.h"
#include "rvfc/system/win32/uuid.h"
#endif // _WIN32

#pragma warning(push)
#pragma warning(disable : 4250)

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

//---------------------------------------------------------------------------------------------

class SharedMemory
{
protected:
	std::string _name;
	HandleX _handle;
	bool is_new;
	void *_memory;

	void allocate(int size, bool writable)
	{
		_handle = CreateFileMapping(HANDLE(0xFFFFFFFF), NULL, 
			(writable ? PAGE_READWRITE : PAGE_READONLY), 0L, size, _name.c_str());
		if (!_handle)
			throw std::runtime_error("SharedMemory: CreateFileMapping failed");
		is_new = GetLastError() != ERROR_ALREADY_EXISTS;
		_memory = (void *) MapViewOfFile(_handle, 
			FILE_MAP_READ | (writable ? FILE_MAP_WRITE : 0), 0, 0, 0 );
		if (!_memory)
			throw std::runtime_error("SharedMemory: MapViewOfFile failed");
	}

	void free()
	{
		if (_memory)
			UnmapViewOfFile((LPVOID) _memory);
		_handle.close();
	}

public:
	SharedMemory(const std::string &name, int size, bool writable = true) : _name(name)
	{
		allocate(size, writable);
	}

	SharedMemory(int size, bool writable = true) : _name(Uuid())
	{
		allocate(size, writable);
	}

	~SharedMemory() { free(); }

    bool isNew() const { return is_new; }
	std::string name() const { return _name; }

	void *data() { return _memory; }
	const void *data() const { return _memory; }

	char &operator[](int i) { return ((char *)_memory)[i]; }
	const char &operator[](int i) const { return ((char *)_memory)[i]; }
};

//---------------------------------------------------------------------------------------------

template <class T>
class SharedObject : public SharedMemory
{
public:
	SharedObject(const string &name, bool writable = true) : SharedMemory(name, sizeof(T), writable)
	{
		if (isNew())
			new (_memory) T();
	}

	SharedObject(bool writable = true) : SharedMemory(sizeof(T), writable)
	{
		if (isNew())
			new (_memory) T();
	}
	
	T *operator->() { return reinterpret_cast<T*>(_memory); }
	const T *operator->() const { return reinterpret_cast<const T*>(_memory); }
};

//---------------------------------------------------------------------------------------------

#endif // _WIN32

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#pragma warning(pop)

#endif
