
///////////////////////////////////////////////////////////////////////////////////////////////
// File
///////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _rvfc_filesys_file_
#define _rvfc_filesys_file_

#include "rvfc/filesys/classes.h"
#include "rvfc/filesys/filename.h"

#include "rvfc/exceptions/defs.h"
// #include "rvfc/types/defs.h"

#include <string>

#ifndef _WINDOWS_
#include <windows.h>
#endif

namespace rvfc
{

using namespace rvfc;
using std::string;

///////////////////////////////////////////////////////////////////////////////////////////////
// File
///////////////////////////////////////////////////////////////////////////////////////////////

class File
{
	Path _path;

	static bool removeFile(const char *file);
	static bool removeDir(const char *file);

public:
	explicit File() : _path("") {}
	File(const File &file) : _path(file._path) {}
	File(Directory &dir, const Filename &filename);
	File(const Path &path) : _path(path) {}
	explicit File(const string &path) : _path(Path(path)) {}

	bool operator!() const { return !_path; }
	Filename name() const { return _path.filename(); }
	const Path &path() const { return _path; }

	bool exist() const;

	void copy(const Path &path, bool force = false);
	void copy(const Directory &dir, bool force = false);

	bool remove(bool force = false);

	FILE *fopen(const char *mode) const;
	FILE *update() const { return fopen("r+"); }
	FILE *rewrite() const { return fopen("w"); }
	FILE *read() const { return fopen("r"); }

	class Info
	{

	};

	Info info() const;
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Files
///////////////////////////////////////////////////////////////////////////////////////////////

class Files
{
	WildcardPath _filespec;

public:
	Files(const WildcardPath &filespec) : _filespec( filespec) {}

	struct IMapFunction
	{
		virtual void operator()(_Directory_::Iterator &i) {}
		virtual void down(_Directory_::Iterator &i) {}
		virtual void up() {}
	};

protected:
	static void preOrderMap(IMapFunction &f, _Directory_::Iterator i);
	static void postOrderMap(IMapFunction &f, _Directory_::Iterator i);

public:
	void preOrderMap(IMapFunction &f);
	void postOrderMap(IMapFunction &f);
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_filesys_file_
