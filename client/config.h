
#ifndef _distcc_config_h_
#define _distcc_config_h_

#include <string>

#include "rvfc/filesys/defs.h"
#include "rvfc/text/defs.h"

#include "common/arg.h"
#include "common/trace.h"

namespace distcc
{

using namespace rvfc::Text;
using rvfc::File;
using rvfc::Directory;

///////////////////////////////////////////////////////////////////////////////////////////////

class ClientConfig
{
public:
	int port;
	text compiler;
	text session;
	text view;
	text hosts_class;
	bool on_server;
	rs_loglevel arg_log_level;
	int log_stderr, log_syslog;
	text log_file; //@@ currently unused
	bool verbose;
	Arguments cc_args;

	mutable Directory root_dir;

	bool parse_options(const Arguments &args);

public:
	ClientConfig();
	
	File hostsFile() const;

	Directory top_dir() const;
	Directory subdir(const string &name) const;
	Directory lock_dir() const;
	Directory state_dir() const;
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif // _distcc_config_h_
