
///////////////////////////////////////////////////////////////////////////////////////////////
// Filename
///////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _rvfc_filesys_filename_
#define _rvfc_filesys_filename_

#include "rvfc/filesys/classes.h"

#include "rvfc/exceptions/defs.h"
#include "rvfc/text/defs.h"

#include <string>

#ifndef _WIN32
#include "rvfc/win.h"
#endif

namespace rvfc
{

using std::string;
using namespace rvfc::Text;

///////////////////////////////////////////////////////////////////////////////////////////////
// Path Components
///////////////////////////////////////////////////////////////////////////////////////////////

class PathComponents
{
protected:
	struct Components
	{
		char volume[_MAX_DRIVE], 
			dir[_MAX_DIR], 
			fname[_MAX_FNAME], 
			ext[_MAX_EXT];
	};

	mutable Components *_components;
	const Components &components() const;
	void refresh();

	virtual const string &pathText() const = 0;

public:
	PathComponents() : _components(0) {}
	~PathComponents() { delete _components; }

	string volume() const { return components().volume; }
	string dir() const { return components().dir; }
	string path() const { return volume() + dir(); }

	string forename() const { return components().fname; }
	string ext() const { return components().ext; }
	string surname() const { return ext(); }
	string filename() const { return forename() + surname(); }
	string basename() const { return filename(); }
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Filename
///////////////////////////////////////////////////////////////////////////////////////////////

class Filename : protected PathComponents
{
	friend UniqueFilename;
	friend Directory;

protected:
	string _name;
	const string &pathText() const { return _name; }

	Filename() {}

public:
	Filename(const string &name ) : _name( name) {}
	Filename(const Filename &filename ) : _name( filename) {}

	void assign(const Filename &filename);
	Filename &operator=(const Filename &filename ) { assign( filename); return *this; }

	Filename forename() const { return PathComponents::forename(); }
	string ext() const { return PathComponents::ext(); }
	string surname() const { return ext(); }
	Path fullPath() const;

	operator string() const { return _name; }
	const char *c_str() const { return _name.c_str(); }
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Path
///////////////////////////////////////////////////////////////////////////////////////////////

class Path : public text, public PathComponents
{
	friend UniqueFilename;
	friend Directory;

protected:
	const text &pathText() const { return *this; }

public:
	Path() {}
	Path(const string &name) : text(name) {}
	Path(const Path &path) : text(path) {}

	Path &operator=(const Path &path) { assign(path); refresh(); return *this; }

	Path volume() const { return PathComponents::volume(); }
	Path dir() const { return PathComponents::dir(); }
	Path path() const { return PathComponents::path(); }

	Filename forename() const { return PathComponents::forename(); }
	text ext() const { return PathComponents::ext(); }
	Filename basename() const { return PathComponents::basename(); }
	Filename filename() const { return basename(); }

	Path real() const;
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Relative Path
///////////////////////////////////////////////////////////////////////////////////////////////

class RelativePath : public string
{
};

///////////////////////////////////////////////////////////////////////////////////////////////
// UniqueFilename
///////////////////////////////////////////////////////////////////////////////////////////////

class UniqueFilename : public Filename
{
	void ctor(const string &type, const Directory &dir);

public:
	UniqueFilename(const string &type);
	UniqueFilename(const string &type, const Directory &dir);
};

///////////////////////////////////////////////////////////////////////////////////////////////
// WildcardFilename
///////////////////////////////////////////////////////////////////////////////////////////////

class WildcardPath : public Path
{
public:
	WildcardPath(const string &name ) : Path( name) {}
	WildcardPath(const Directory &dir, const string &wildcard = "*.*");
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_filesys_filename_
