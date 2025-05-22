
#include "common/config.h"

#include "rvfc/text/defs.h"

#include "common/distcc.h"
#include "client/config.h"

using namespace rvfc;

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

ClientConfig::ClientConfig()
{
	port = DISTCC_DEFAULT_PORT;
	arg_log_level = RS_LOG__INVALID;
}

//---------------------------------------------------------------------------------------------

File ClientConfig::hostsFile() const
{
	File hosts(top_dir()/"hosts");
	if (!hosts.exist())
	{
        rs_trace("not reading %s", +hosts.name());
		File hosts1(stringf("%s/distcc/hosts", SYSCONFDIR));
		if (!hosts1.exist())
		{
	        rs_trace("not reading %s", hosts1.name().c_str());
			throw "Cannot file hosts file";
		}
		return hosts1;
	}
	return hosts;
}

//---------------------------------------------------------------------------------------------
// Return a static string holding DISTCC_DIR, or ~/.distcc.
// The directory is created if it does not exist.

Directory ClientConfig::top_dir() const
{
    if (!!root_dir) 
        return root_dir;

	char *distcc_dir = getenv("DISTCC_DIR");
    if (distcc_dir) 
		return root_dir = Path(distcc_dir);

#if defined(__linux__)
    distcc_dir = getenv("HOME");
#elif defined(_WIN32)
	distcc_dir = getenv("USERPROFILE");
#endif
	if (!distcc_dir)
	{
		rs_log_warning("HOME is not set; can't find distcc directory");
		throw "dcc_get_top_dir: HOME is not set; can't find distcc directory";
	}

	Directory dir = stringf("%s/.distcc", distcc_dir);
	if (!dir.exist())
		dir.create();
	return root_dir = dir;
}

//---------------------------------------------------------------------------------------------
// Return a subdirectory of the DISTCC_DIR of the given name, making sure that the directory exists.

Directory ClientConfig::subdir(const string &name) const
{
	Directory dir = top_dir().subdir(name);
	dir.create();
	return dir;
}

//---------------------------------------------------------------------------------------------

Directory ClientConfig::lock_dir() const
{
    static Directory cached;
    if (!!cached) 
        return cached;

	return cached = subdir("lock");
}

//---------------------------------------------------------------------------------------------

Directory ClientConfig::state_dir() const
{
    static Directory cached;
    if (!!cached) 
        return cached;

	return cached = subdir("state");
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
