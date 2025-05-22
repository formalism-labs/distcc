
#include "filename.h"
#include "directory.h"

#include "exceptions/defs.h"

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////
// PathComponents
///////////////////////////////////////////////////////////////////////////////////////////////

const PathComponents::Components &PathComponents::components() const
{
	if (!_components)
	{
		_components = new Components;
		Components &c = *_components;
		_splitpath( reinterpret_cast<const char *>(+pathText()), 
			c.volume, c.dir, c.fname, c.ext );
	}
	return *_components;
}

//---------------------------------------------------------------------------------------------

void PathComponents::refresh()
{
	delete _components;
	_components = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Filename
///////////////////////////////////////////////////////////////////////////////////////////////

void Filename::assign(const Filename &filename)
{
	_name.assign(filename);
	refresh();
}

//---------------------------------------------------------------------------------------------

Path Filename::fullPath() const
{
	char path[MAX_PATH];
	LPTSTR file_part;
	DWORD rc = GetFullPathName(+_name, sizeof(path), path, &file_part);
	return path;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Path
///////////////////////////////////////////////////////////////////////////////////////////////

Path Path::real() const
{
#ifdef _WIN32
	char *p = _fullpath(0, c_str(), 0);
#else
	char *p = realpath(c_str(), 0);
#endif
	if (!p)
		throw error("cannot determine real path for '%s' at '%s'", c_str(), +CurrentDirectory().path());
	Path real = p;
	free(p);
	return real;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// UniqueFilename
///////////////////////////////////////////////////////////////////////////////////////////////

void UniqueFilename::ctor(const string &type, const Directory &dir)
{
	char name[MAX_PATH];
	int rc = GetTempFileName(+dir.path(), "tmp", 0, name);
	if (!rc)
		throw std::runtime_error("cannot generate temp filename");

	assign(string( name ) + "." + type);
}

//---------------------------------------------------------------------------------------------

UniqueFilename::UniqueFilename(const string &type)
{
	ctor(type, TemporaryDirectory());
}

//---------------------------------------------------------------------------------------------

UniqueFilename::UniqueFilename(const string &type, const Directory &dir)
{
	ctor(type, dir);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// UniqueFilename
///////////////////////////////////////////////////////////////////////////////////////////////

WildcardPath::WildcardPath(const Directory &dir, const string &wildcard) :
	Path(dir.subpath( wildcard ))
{
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc
