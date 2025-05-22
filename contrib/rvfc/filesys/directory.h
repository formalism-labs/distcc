
///////////////////////////////////////////////////////////////////////////////////////////////
// Directory
///////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _rvfc_filesys_directory_
#define _rvfc_filesys_directory_

#include "rvfc/filesys/classes.h"

// #include "rvfc/types/defs.h"
#include "rvfc/filesys/filename.h"
#include "rvfc/filesys/file.h"

#include <string>

#ifdef _WIN32
#include "rvfc/win.h"

typedef int mode_t;
#endif

namespace rvfc
{

using std::string;

///////////////////////////////////////////////////////////////////////////////////////////////
// Directory
///////////////////////////////////////////////////////////////////////////////////////////////

class Directory
{
	friend NewDirectory;
	friend TemporaryDirectory;
	friend CurrentDirectory;

	Path _path;

public:
	Directory() {}
	Directory(const Path &path) : _path(path) {}
	explicit Directory(const string &path) : _path(Path(path)) {}

	const Path &path() const { return _path; }

	bool operator!() const { return !_path; }

	bool exist() const;

	File file(const Filename &filename) const;
	File operator[](const Filename &filename) const { return file(filename); }
	File operator[](const string &filename) const { return file(Filename(filename)); }
	File operator/(const string &path) const { return file(path); }
	
	Directory subdir(const Path &path) const;
	Directory operator()(const string &path) const { return subdir(Path(path)); }

	Path subpath(const string &name) const;

	void create();
	void create(mode_t mode, bool strict = true);
	void copy(Directory &dir);

	typedef _Directory_::Iterator Iterator;
};

///////////////////////////////////////////////////////////////////////////////////////////////

class NewDirectory : public Directory
{
public:
	NewDirectory(Path &name);
};

///////////////////////////////////////////////////////////////////////////////////////////////

class TemporaryDirectory : public Directory
{
public:
	TemporaryDirectory();
};

///////////////////////////////////////////////////////////////////////////////////////////////

class CurrentDirectory : public Directory
{
public:
	CurrentDirectory();
};

///////////////////////////////////////////////////////////////////////////////////////////////

namespace _Directory_
{

class Iterator
{
public:
	class File : protected WIN32_FIND_DATA
	{
		friend Iterator;
		Iterator &_iterator;

	protected:
		File(Iterator &iterator ) : _iterator(iterator) {}
		File(Iterator &iterator, WIN32_FIND_DATA &find_data) : 
			_iterator(iterator ), WIN32_FIND_DATA(find_data) {}

	public:
		Filename name() const;
		Path path() const;

		DWORD attributes() const { return dwFileAttributes; }
		bool isDir() const { return !!(attributes() & FILE_ATTRIBUTE_DIRECTORY); }
	};

	friend File;

private:
	WildcardPath _dirspec;
	HANDLE _handle;
	File _file;
	bool _valid;

	void ctor();

public:
	Iterator(const WildcardPath &dirspec);
	Iterator(Iterator &iterator, File &file);
	~Iterator();

	Iterator &operator++();
	bool operator!() const;

	File operator*() { return _file; }
	File *operator->() { return &_file; }

	rvfc::File file() const;

	Path path() const { return _file.path(); }
	Path commonPath() const { return _dirspec.path(); }
};

} // namespace _Directory_

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_filesys_directory_
