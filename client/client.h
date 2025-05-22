
#ifndef _distcc_client_client_h_
#define _distcc_client_client_h_

#include <string>
#include <list>

#include "common/distcc.h"
#include "common/arg.h"
#include "client/config.h"

#include "rvfc/defs.h"

namespace distcc
{

using rvfc::Path;

///////////////////////////////////////////////////////////////////////////////////////////////

class Client
{
	ClientConfig config;

	int sg_level; // recursion safeguard

	static void catch_signals();

	void configure_trace_level();

public:
	Client(const ClientConfig &config);
	~Client();

	int run(const Arguments &args);

	int compile_local(const Arguments &args);
	int compile_remote(Arguments &args, const File &cpp_fname,
		proc_t cpp_pid, dcc_hostdef &host, int &status);

	int build_somewhere(Arguments &args, int sg_level, int &status);
	int build_somewhere_timed(Arguments &args, int sg_level, int &status);
	int build_fallback(Arguments &args, dcc_hostdef *host);

	int support_masquerade(const Arguments &args, const string &progname, int &did_masquerade);

	int cpp_maybe(Arguments &args, File &cpp_fname, proc_t &cpp_pid);

	int retrieve_results(fd_t net_fd, int &status, Arguments &args, dcc_hostdef &host);

	dcc_hostdef lock_local(int &cpu_lock_fd);
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif // _distcc_client_client_h_
