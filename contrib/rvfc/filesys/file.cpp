
#if !defined(_WIN32) && !defined(__linux__)
#error No implementation for this platform.
#endif

#include "file.h"

#include "rvfc/filesys/defs.h"
#include "rvfc/text/defs.h"

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////
// File
///////////////////////////////////////////////////////////////////////////////////////////////

File::File(Directory &dir, const Filename &filename) :
	_path(dir.file(filename).path())
{
}

//---------------------------------------------------------------------------------------------

bool File::exist() const
{
	Directory::Iterator i(path());
	return !!i;
}

//---------------------------------------------------------------------------------------------

void File::copy(const Path &destPath, bool force)
{
	BOOL rc = ::CopyFile(+_path, +destPath, force);
	if (!rc)
		throw std::runtime_error(stringf("cannot copy file %s to %s", +path(), +destPath));
}

//---------------------------------------------------------------------------------------------

void File::copy(const Directory &dir, bool force)
{
	copy(dir[path().basename()].path(), force);
}

//---------------------------------------------------------------------------------------------

bool File::remove(bool force)
{
	struct F : public Files::IMapFunction
	{
		bool rc;

		F() : rc(true) {}

		void operator()(Directory::Iterator &di)
		{
			Path path = di.path();
			if (di->isDir())
				rc = removeDir(+path) && rc;
			else
				rc = removeFile(+path) && rc;
		}
	};

	bool rc = removeFile(+_path);
	if (!rc)
		rc  = removeDir(+_path);
	if (!rc && force)
	{
		F f;
		Files(WildcardPath(_path, "*")).postOrderMap(f);
		rc = f.rc && !!removeDir(+_path);
	}

	return rc;

}

//---------------------------------------------------------------------------------------------

// fail iff file exists but cannot be removed. non-existent files are considered removed.

bool File::removeFile(const char *file)
{
#if defined(_WIN32)
	return !!::DeleteFile(file);

#elif defined(__linux__)
	return unlink(file) == 0 || errno == ENOENT;
#endif
}

//---------------------------------------------------------------------------------------------

bool File::removeDir(const char *file)
{
#if defined(_WIN32)
	return !!::RemoveDirectory(file);

#elif defined(__linux__)
	return rmdir(file) == 0 || errno == ENOENT;
#endif
}

//---------------------------------------------------------------------------------------------

FILE *File::fopen(const char *mode) const
{
	return ::fopen(+_path, mode);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Files
///////////////////////////////////////////////////////////////////////////////////////////////

void Files::preOrderMap(IMapFunction &f, Directory::Iterator i)
{
	f.down(i);
	for (; !!i; ++i)
	{
		f(i);
		if (i->isDir())
			preOrderMap(f, Directory::Iterator(i, *i));
	}
	f.up();
}

//---------------------------------------------------------------------------------------------

void Files::preOrderMap(IMapFunction &f)
{
	preOrderMap(f, Directory::Iterator(_filespec));
}

//---------------------------------------------------------------------------------------------

void Files::postOrderMap(IMapFunction &f, Directory::Iterator i)
{
	f.down(i);
	for (; !!i; ++i)
	{
		if (i->isDir())
			postOrderMap(f, Directory::Iterator(i, *i));
		f(i);
	}
	f.up();
}

//---------------------------------------------------------------------------------------------

void Files::postOrderMap(IMapFunction &f)
{
	postOrderMap(f, Directory::Iterator(_filespec));
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc
