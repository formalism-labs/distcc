
#if !defined(_WIN32) && !defined(__linux__)
#error No implementation for this platform.
#endif

#include "directory.h"

#include "filename.h"
#include "file.h"

#include <errno.h>

#if defined(_WIN32)
#include <direct.h>

#elif defined(__linux__)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace rvfc
{

using namespace _Directory_;

///////////////////////////////////////////////////////////////////////////////////////////////
// Directory
///////////////////////////////////////////////////////////////////////////////////////////////

bool Directory::exist() const
{
	Directory::Iterator i(path());
	return !!i;
}

//---------------------------------------------------------------------------------------------

File Directory::file(const Filename &name) const
{
	return File(subpath( name ));
}

//---------------------------------------------------------------------------------------------

Directory Directory::subdir(const Path &path) const
{
	return Directory(subpath( path ));
}

//---------------------------------------------------------------------------------------------

Path Directory::subpath(const string &filespec) const
{
	return Path(path() + "/" + filespec);
}

//---------------------------------------------------------------------------------------------

void Directory::create()
{
#ifdef __linux__
    if (mkdir(+_path, 0755) == 0 || errno == EEXIST) 
		return;
#elif defined(_WIN32)
    if (_mkdir(+_path) == 0 || errno == EEXIST) 
		return;
#endif
	throw std::runtime_error("cannot create directory");
}

//---------------------------------------------------------------------------------------------
// if strict and dir exists, fail if actual and requested permissions don't match

void Directory::create(mode_t mode, bool srict)
{
#if defined(_WIN32)
	create();
#elif defined(__linux__)
    if (mkdir(p, mode) == 0)
		return;
	if (errno != EEXIST)
		throw std::runtime_error("cannot create directory");
	if (!strict)
		return;
	struct stat st;
	if (stat(+_path, &st))
		throw std::runtime_error("cannot stat directory");
	if (st.st_mode != mode)
		throw std::runtime_error("directory exists but permissions don't match");
#endif
}

//---------------------------------------------------------------------------------------------

void Directory::copy(Directory &dir)
{
	struct F : Files::IMapFunction
	{
		Directory src_dir, dest_dir;
		int _bias;

		F(Directory &srcDir, Directory &destDir ) : src_dir( srcDir ), dest_dir( destDir) 
		{
			_bias = src_dir.path().length() + 1;
		}

		void operator()(Iterator &i)
		{
			try
			{
				Path dest(dest_dir.path() + "/" + i.path().substr(_bias));
				if (i->isDir())
				{
					NewDirectory new_dir(dest);
				}
				else
					File(i.path()).copy( dest);
			} catch (...) {}
		}
	};

	Files(WildcardPath(*this)).preOrderMap(F(*this, dir));
}

///////////////////////////////////////////////////////////////////////////////////////////////
// NewDirectory
///////////////////////////////////////////////////////////////////////////////////////////////

NewDirectory::NewDirectory(Path &path) : Directory(path)
{
	create();
}

///////////////////////////////////////////////////////////////////////////////////////////////
// TemporaryDirectory
///////////////////////////////////////////////////////////////////////////////////////////////

TemporaryDirectory::TemporaryDirectory()
{
	char dir[1024];
	DWORD rc = GetTempPath(sizeof(dir)-1, dir);
	_path.assign(dir);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// CurrentDirectory
///////////////////////////////////////////////////////////////////////////////////////////////

CurrentDirectory::CurrentDirectory()
{
	char path[MAX_PATH];
	char *p;
#if defined(_WIN32)
	p = _getcwd(path, sizeof(path));
#elif defined(__linux__)
	p = getcwd(path, sizeof(path));
#endif
	if (!p)
		throw std::runtime_error("cannot determine current directory");
	_path.assign(path);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Directory Iterator
///////////////////////////////////////////////////////////////////////////////////////////////

namespace _Directory_
{

///////////////////////////////////////////////////////////////////////////////////////////////

void Iterator::ctor()
{
	string d = _dirspec;
	if (*d.begin() == '\"' && *d.rbegin() == '\"')
		d = d.substr(1, d.length() - 2);
	_handle = FindFirstFile(+d, &_file);
	_valid = _handle != INVALID_HANDLE_VALUE;
	while (_valid && (!strcmp(_file.cFileName, ".") || !strcmp(_file.cFileName, "..")))
		++(*this);
}

//---------------------------------------------------------------------------------------------

Iterator::Iterator(const WildcardPath &dirspec) : 
	_dirspec(dirspec), _file(*this)
{
	ctor();
}

//---------------------------------------------------------------------------------------------

Iterator::Iterator(Iterator &iterator, File &file) :
	_dirspec(string(iterator.path()) + string(file.name()) + "\\*.*"),
	_file(*this)
{
	ctor();
}

//---------------------------------------------------------------------------------------------

Iterator::~Iterator()
{
	FindClose(_handle);
}

///////////////////////////////////////////////////////////////////////////////////////////////

Iterator &Iterator::operator++()
{
	_valid = !!FindNextFile(_handle, &_file);
	return *this;
}

//---------------------------------------------------------------------------------------------

bool Iterator::operator!() const
{
	return !_valid;
}

///////////////////////////////////////////////////////////////////////////////////////////////

rvfc::File Iterator::file() const
{
	return rvfc::File(path());
}

///////////////////////////////////////////////////////////////////////////////////////////////
// DirectoryIterator :: File
///////////////////////////////////////////////////////////////////////////////////////////////

Filename Iterator::File::name() const 
{ 
	return cFileName; 
}

//---------------------------------------------------------------------------------------------

Path Iterator::File::path() const 
{ 
	return Directory(_iterator._dirspec.dir()).subpath(name());
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace _Directory_

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc
