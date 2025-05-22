
#include "common/config.h"

#include <signal.h>

#include "common/compiler.h"

#include "client/client.h"
#include "client/dopt.h"
#include "client/implicit.h"
#include "common/lock.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

Client::Client(const ClientConfig &config) : config(config)
{
#ifdef __linux__
	dcc_client_catch_signals();
	dcc_ignore_sigpipe(1); // Ignore SIGPIPE; we consistently check error codes and will see the EPIPE
#endif

	atexit(dcc_cleanup_tempfiles);
	atexit(dcc_remove_state_file);

	dcc_state_dir = config.state_dir(); //@@ global var for state files mechanism
	lock_dir = config.lock_dir(); // @@ global var for dcc_hostdef lock file

	configure_trace_level();

	dcc_trace_version();

	sg_level = dcc_recursion_safeguard();
}

//---------------------------------------------------------------------------------------------

Client::~Client()
{
}

//---------------------------------------------------------------------------------------------

int Client::run(const Arguments &args)
{
	int status, tweaked_path = 0;
	int ret;
	Arguments &cc_args = config.cc_args;

	dcc_set_compiler(cc_args, 0);

	File program = args[0];
	text compiler_name = program.path().basename();

	rs_trace("compiler name is \"%s\"", +compiler_name);

	// Either "distcc -c hello.c" or "distcc gcc -c hello.c"
	Arguments compiler_args;
	if (compiler_name.contains("distcc"))
	{
		dcc_find_compiler(cc_args, compiler_args);
		// compiler_args is now respectively either "cc -c hello.c" or "gcc -c hello.c"

#if 0
		// I don't think we need to call this: if we reached this line, our invocation name 
		// is something like 'distcc', and that's never a problem for masquerading loops.
		if ((ret = dcc_trim_path(compiler_name)) != 0)
			goto out;
#endif
	}
	else 
	{
		// Invoked as "cc -c hello.c", with masqueraded path
		if ((ret = support_masquerade(cc_args, compiler_name, tweaked_path)) != 0)
			throw "cannot determine masquerade mode";
		
		compiler_args = cc_args;
		compiler_args[0] = compiler_name;
	}

	if (sg_level - tweaked_path > 0) 
	{
		rs_log_crit("distcc seems to have invoked itself recursively!");
		ret = EXIT_RECURSION;
		throw "distcc seems to have invoked itself recursively!";
	}

	return build_somewhere_timed(compiler_args, sg_level, status);
}
	
//---------------------------------------------------------------------------------------------

static RETSIGTYPE dcc_client_signalled (int whichsig)
{
	signal(whichsig, SIG_DFL);

#ifdef HAVE_STRSIGNAL
	rs_log_info("%s", strsignal(whichsig));
#else
	rs_log_info("terminated by signal %d", whichsig);
#endif

	dcc_cleanup_tempfiles();

	raise(whichsig);
}

//---------------------------------------------------------------------------------------------

void Client::catch_signals()
{
	signal(SIGTERM, &dcc_client_signalled);
	signal(SIGINT, &dcc_client_signalled);
#ifdef __linux__
	signal(SIGHUP, &dcc_client_signalled);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
