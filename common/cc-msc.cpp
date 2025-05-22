
#include "config.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <regex>

#include <sys/stat.h>

#include "distcc.h"
#include "arg.h"
#include "trace.h"
#include "util.h"
#include "exitcode.h"

#include "compiler.h"

namespace distcc
{

using namespace std::tr1;

///////////////////////////////////////////////////////////////////////////////////////////////
// MSC stuff
///////////////////////////////////////////////////////////////////////////////////////////////

string MscCompiler::preproc_exten(const string &e) const
{
	// c/c++ both return .i as a preprocessed file. cl cannot distinguish between a c/++ 
	// preprocessed file so it treats it as an object file.

	return dcc_find_extension(e);
}

//---------------------------------------------------------------------------------------------

bool MscCompiler::is_source(const string &sfile) const
{
    text ext = dcc_find_extension(sfile);
    if (!ext)
        return false;
    return ext.equalsto_one_of(".c", ".cc", ".cpp", ".cxx", 0);
}

//---------------------------------------------------------------------------------------------

bool MscCompiler::is_preprocessed(const string &sfile) const
{
	string ext = dcc_find_extension(sfile);
	if (ext.empty())
		return false;
	return ext == ".i";
}

//---------------------------------------------------------------------------------------------
// Decide whether @p filename is an object file, based on its extension.
// @note: CL specific function

bool MscCompiler::is_object(const string &filename) const
{
    string ext = dcc_find_extension(filename);
    if (ext.empty())
        return false;

    return ext == ".obj";
}

///////////////////////////////////////////////////////////////////////////////////////////////
// MSC (CL) stuff
///////////////////////////////////////////////////////////////////////////////////////////////

// input_file : the source file
// output_file : the name of the file the source is compiled into (eg. .obj)
// ret_newargs : A copy of the arguments with added /Fo option
// This function is poorly done. You'll probably get away with most things, but I do not have 
// the experience with CL to make these choices. I try to interpret their cryptic descriptions.

void MscCompiler::scan_args(Arguments &args, bool on_server)
{
    bool seen_opt_object = false;    // if we see the /Fo arguement
    bool seen_opt_asm = false;       // if we see the /Fa arguement
    bool seen_opt_c = false;         // if we see the /c arguement

	Arguments args0 = args; // for diagnostics

    args.trace("scanning arguments");

    // Things like "distcc -c hello.c" with an implied compiler are handled earlier on by 
	// inserting a compiler name.  At this point, argv[0] should always be a compiler name.
    if (args[0][0] == '-')
	{
        rs_log_error("unrecognized distcc option: %s", +args[0]);
		throw "MscCompiler::scan_args: bad arguments";
    }

	for (Arguments::Iterator i = args; !!i; ++i)
	{
		text a = *i;

		if (a[0] == '-')
		{
			if (a == "-E") 
			{
				rs_trace("/E call for cpp must be local");
				throw "MscCompiler::scan_args: error";
			}
			else if (a.startswith("-Fa")) 
			{
				seen_opt_asm = true;
				if (a[3] != '\0') 
				{
					args.found_output_file(Arguments::convert_win_path(a.substr(3)));
					*i = a.substr(0, 3) + args.output_file;
				}
			}
			else if (a.startswith("-Fo")) 
			{
				seen_opt_object = true;
				if (a[3] != '\0') 
				{
					args.found_output_file(Arguments::convert_win_path(a.substr(3)));
					*i = a.substr(0, 3) + args.output_file;
				}
			}
			else if (a.startswith("-I"))
			{
				*i = a.substr(0, 2) + Arguments::convert_win_path(a.substr(2));
			}
			else if (a.startswith("-Fd")) 
			{
				if (a[3] != '\0')
				{
					args.pdb_file = Arguments::convert_win_path(a.substr(3));
					*i = a.substr(0, 3) + args.pdb_file;
				}
			}
			else if (a == "-c") 
			{
				seen_opt_c = true;
			} 
			else if (!a.find("-Fe")) 
			{
				// Ignore this
			}
		}
        else 
		{
            if (is_source(a)) 
			{
				rs_trace("found input file '%s'", a);
                if (!!args.input_file) 
				{
					rs_log_info("do we have two inputs?  i give up");
					throw "MscCompiler::scan_args: error";
                }

				args.input_file = Arguments::convert_win_path(a);
				*i = args.input_file;

//				dcc_trace_argv("check argv after trying input file conversion", args0);
//				dcc_trace_argv("check retnewargv after trying input file conversion", args);
            }
            else if (a.endswith(".obj")) 
			{
				args.found_output_file(a);
            }
        }
    }

    // TODO: ccache has the heuristic of ignoring arguments that are not
    // extant files when looking for the input file; that's possibly worthwile.  
	// Of course we can't do that on the server.

    if (!seen_opt_c) 
	{
        rs_log_info("compiler apparently called not for compile");
		throw "MscCompiler::scan_args: error";
    }

    if (args.input_file.empty()) 
	{
        rs_log_info("no visible input file");
		throw "MscCompiler::scan_args: error";
    }

    if (args.source_needs_local())
		throw "MscCompiler::scan_args: error";

    if (args.output_file.empty()) 
	{
		string ofile;

		if (seen_opt_asm) 
		{
			if (dcc_output_from_source(args.input_file, ".asm", ofile))
				throw "MscCompiler::scan_args: error";
		}
		else if (seen_opt_object || seen_opt_c) 
		{
			if (dcc_output_from_source(args.input_file, ".obj", ofile))
				throw "MscCompiler::scan_args: error";
		}
		else 
		{
			rs_log_crit("this can't be happening(%d)!", __LINE__);
			throw "MscCompiler::scan_args: error";
		}

		rs_log_info("no visible output file, going to add '-Fo%s' at end", +ofile);

		args += stringf("%s%s", seen_opt_asm ? "-Fa" : "-Fo", +ofile);
		args.output_file = ofile;
	}
/*	
	if (!seen_opt_c) 
	{
		rs_log_info("No /c arguement found. Adding it.");
		dcc_argv_append(argv, strdup("/c"));
    } 
*/

    dcc_note_compiled(args.input_file, args.output_file);

    if (args.output_file[0] == '-') 
	{
		// Different compilers may treat "-o -" as either "write to stdout", or "write to a file called '-'".  
		// We can't know, so we just always run it locally. 
		// Hopefully this is a pretty rare case.
        rs_log_info("output to stdout?  running locally");
		throw "MscCompiler::scan_args: error";
    }
}

//---------------------------------------------------------------------------------------------
// We change it to /E if before we process and before we assemble

int MscCompiler::set_action_opt(Arguments &args, const string &new_c)
{
    bool gotone = false;
    
	for (Arguments::Iterator i = args; !!i; ++i)
	{
		text a = *i;

		if (a == "-c" || a.startswith("-FA"))
		{
            *i = new_c;
            gotone = true;
        }
	}

    if (!gotone)
	{
        rs_log_error("failed to find -c or -FA");
        return EXIT_DISTCC_FAILED;
    }

	return 0;
}

//---------------------------------------------------------------------------------------------

// Change object file or suffix of -o to @p ofname
//
// It's crucially important that in every case where an output file is detected by dcc_scan_args(), 
// it's also correctly identified here.
// It might be better to make the code shared.

int MscCompiler::set_output(Arguments &args, const Path &o_fname, const Path &dotd_fname, const Path &pdb_fname)
{
	args.trace("command before");

	if (!args.replace_file_args("-Fo", o_fname, Arguments::Find_OneOpt))
	{
		rs_log_error("failed to find '-Fo'");
		return EXIT_DISTCC_FAILED;
	}

	args.replace_file_args("-Fd", pdb_fname, Arguments::Find_OneOpt);

	args.trace("command after");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// MSC stuff
///////////////////////////////////////////////////////////////////////////////////////////////

// Strip arguments like -D, -I, and /clr from a command line, because they do not need to be 
// passed across the wire.  This covers options for both the preprocess and link phases, since 
// they should never happen remotely.
//
// In the case where we inadvertently do cause preprocessing to happen remotely, it is possible 
// that omitting these options will make failure more obvious and avoid false success.
//
// Giving -L on a compile-only command line is a bit wierd, but it is observed to happen in 
// Makefiles that are not strict about CFLAGS vs LDFLAGS, etc.
//
// NOTE: a categorical listing of all of cl's options are listed at:
// http://msdn2.microsoft.com/en-us/library/19z1t1wy(VS.80).aspx
// All preprocessor options done.

void MscCompiler::strip_local_args(Arguments &args, bool on_server)
{
	if (!on_server)
		return;

    // skip through argv, copying all arguments but skipping ones that ought to be omitted
	for (Arguments::Iterator i = args; !!i; ++i)
	{
		text a = *i;

		if (a.equalsto_one_of("-I", "-U", "-FI", "-U", "-Fx", "-F", 0) || a.startswith("-Fa"))
		{
			i.remove(2);
		}
		else if (a.startswith_one_of(
				"-D", "-U", "-I", "-AI", "-C", "-D", "-E", "-FI", "-FU", "-U", "-Fe", 
				"-link", "@", "-FA", "-Fm", "-FR", "-Fr", "-Y", 0))
		{
			// Something like "-DNDEBUG" or "-Wp,-MD,.deps/nsinstall.pp".  Just skip this word.
			i.remove();
		}
		else if (a.equalsto_one_of("-E", "-u", "-X", "-LN", "-doc", "-Zs", 0)) 
		{
			// Options that only affect cpp; skip
			i.remove();
		}
    }
    
    args.trace("result");
}

//---------------------------------------------------------------------------------------------

// Remove "/Fo" options from argument list.
//
// This is used when running the preprocessor, when we just want it to write
// to stdout, which is the default when no -o option is specified.
//
// Structurally similar to dcc_strip_local_args()

void MscCompiler::strip_dasho(Arguments &args)
{
    // skip through argv, copying all arguments but skipping ones that ought to be omitted

	for (Arguments::Iterator i = args; !!i; ++i)
        if (i->startswith("-Fo")) // skip "-Foc:\path\to\FILE.obj"
			i.remove();
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
