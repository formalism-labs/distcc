
#include "config.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>

#include "distcc.h"
#include "arg.h"
#include "trace.h"
#include "util.h"
#include "exitcode.h"

#include "compiler.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////
// Diab stuff
///////////////////////////////////////////////////////////////////////////////////////////////

string DiabCompiler::preproc_exten(const string &e) const
{
	// c/c++ both return .i as a preprocessed file. diab cannot distinguish between a c/++ 
	// preprocessed file so it treats it as an object file.

	return dcc_find_extension(e);
}

//---------------------------------------------------------------------------------------------

bool DiabCompiler::is_source(const string &sfile) const
{
	text ext = dcc_find_extension(sfile);
    return ext.equalsto_one_of(".c", ".cc", ".cpp", ".cxx", 0);
}

//---------------------------------------------------------------------------------------------

bool DiabCompiler::is_preprocessed(const string &sfile) const
{
    string ext = dcc_find_extension(sfile);
	return ext == ".i";
}

//---------------------------------------------------------------------------------------------
// Decide whether @p filename is an object file, based on its extension.
// @note: CL specific function

bool DiabCompiler::is_object(const string &filename) const
{
    return dcc_find_extension(filename) ==  ".o";
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Diab stuff
///////////////////////////////////////////////////////////////////////////////////////////////

// Parse arguments, extract ones we care about, and also work out whether it will be 
// possible to distribute this invocation remotely.
//
// This is a little hard because the cc argument rules are pretty complex, but the function 
// still ought to be simpler than it already is.
//
// This code is called on both the client and the server, though they use the results differently.
//
// @returns 0 if it's ok to distribute this compilation, or an error code.

void DiabCompiler::scan_args(Arguments &args, bool on_server)
{
	bool seen_opt_c = false, seen_opt_s = false;

    args.trace("scanning arguments");

    // Things like "distcc -c hello.c" with an implied compiler are handled earlier on by 
	// inserting a compiler name.  At this point, argv[0] should always be a compiler name.
    if (args[0][0] == '-') 
	{
        rs_log_error("unrecognized distcc option: %s", +args[0]);
        throw "GccCompiler::scan_args: bad arguments";
    }

	for (Arguments::Iterator i = args; !!i; ++i)
	{
		text a = *i;
		text::regex::match m;

        if (a[0] == '-') 
		{
            if (a == "-E") 
			{
                rs_trace("-E call for cpp must be local");
                throw "GccCompiler::scan_args: error";
            }
			else if (a.match("-Xmake-dependency-savefile=(.+)", m))
			{
				args.dotd_file = (string) m[1];
			}
			else if (a.startswith("-Xmake-dependency"))
			{
                // Generate dependencies as a side effect. They should work with the way we call cpp.
            }
			else if (a == "-c") 
			{
                seen_opt_c = true;
            }
			else if (a == "-o")
			{
                // Whatever follows must be the output
                args.found_output_file(*++i);
            }
			else if (a.match("-o(.+)", m))
			{
				args.found_output_file(m[1]);
            }
        } 
		else 
		{
            if (is_source(a)) 
			{
                rs_trace("found input file '%s'", +a);
                if (!!args.input_file) 
				{
                    rs_log_info("do we have two inputs? i give up");
                    throw "DiabCompiler::scan_args: error";
                }
                args.input_file = a;
            } 
			else if (a.endswith(".o")) 
			{
				args.found_output_file(a);
            }
        }
    }

    // TODO: ccache has the heuristic of ignoring arguments that are not extant files when 
	// looking for the input file; that's possibly worthwile.  Of course we can't do that on the server.

    if (!seen_opt_c && !seen_opt_s) 
	{
        rs_log_info("compiler apparently called not for compile");
        throw "DiabCompiler::scan_args: error";
    }

    if (!args.input_file)
	{
        rs_log_info("no visible input file");
        throw "DiabCompiler::scan_args: error";
    }

    if (args.source_needs_local())
        throw "DiabCompiler::scan_args: error";

    if (!args.output_file)
	{
        // This is a commandline like "gcc -c hello.c".  They want hello.o, but they don't say so.  
		// For example, the Ethereal makefile does this. 
		//
		// Note: this doesn't handle a.out, the other implied filename, but that doesn't matter 
		// because it would already be excluded by not having -c or -S.

        string ofile;

        // -S takes precedence over -c, because it means "stop after preprocessing" rather 
		// than "stop after compilation."
        if (seen_opt_s) 
		{
            if (dcc_output_from_source(args.input_file, ".s", ofile))
                throw "DiabCompiler::scan_args: error";
        }
		else if (seen_opt_c) 
		{
            if (dcc_output_from_source(args.input_file, ".o", ofile))
                throw "DiabCompiler::scan_args: error";
        }
		else 
		{
            rs_log_crit("this can't be happening(%d)!", __LINE__);
            throw "DiabCompiler::scan_args: error";
        }

        rs_log_info("no visible output file, going to add '-o %s' at end", +ofile);
		args << "-o" << ofile;
        args.output_file = ofile;
    }

    dcc_note_compiled(args.input_file, args.output_file);
}

//---------------------------------------------------------------------------------------------
// Used to change "-c" or "-S" to "-E", so that we get preprocessed source.

int DiabCompiler::set_action_opt(Arguments &args, const string &new_c)
{
    bool gotone = false;
    
	for (Arguments::Iterator i = args; !!i; ++i)
	{
		const string a = *i;

		if (a == "-c" || a == "-S")
		{
            *i = new_c;
            gotone = true;
			// keep going; it's not impossible they wrote "gcc -c -c -c hello.c"
        }
	}

    if (!gotone)
	{
        rs_log_error("failed to find -c or -S");
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

int DiabCompiler::set_output(Arguments &args, const Path &o_fname, const Path &dotd_fname, const Path &pdb_fname)
{
	args.trace("command before");

	Arguments::Iterator i = args.find("-o");
	string orig_o_fname = !!i ? i[1] : "";

	if (!args.replace_file_args("-o", o_fname, Arguments::Find_OneOpt | Arguments::Find_TwoOpt))
	{
		rs_log_error("failed to find '-o'");
		return EXIT_DISTCC_FAILED;
	}

	args.replace_file_args("-Xmake-dependency-savefile=", dotd_fname, Arguments::Find_OneOpt);

	args << "-Xmake-dependency-target=" << orig_o_fname;

	args.trace("command after");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Diab stuff
///////////////////////////////////////////////////////////////////////////////////////////////

// Strip arguments like -D and -I from a command line, because they do not need to be passed 
// across the wire.  This covers options for both the preprocess and link phases, since they 
// should never happen remotely.
//
// In the case where we inadvertently do cause preprocessing to happen remotely, it is possible 
// that omitting these options will make failure more obvious and avoid false success.
//
// Giving -L on a compile-only command line is a bit wierd, but it is observed to happen in 
// Makefiles that are not strict about CFLAGS vs LDFLAGS, etc.
//
// NOTE: gcc-3.2's manual in the "preprocessor options" section describes some options, 
// such as -d, that only take effect when passed directly to cpp.  When given to gcc they 
// have different meanings.

void DiabCompiler::strip_local_args(Arguments &args, bool on_server)
{
	if (!on_server)
		return;

    // skip through argv, copying all arguments but skipping ones that ought to be omitted
	for (Arguments::Iterator i = args; !!i; ++i)
	{
		text a = *i;
		if (a.equalsto_one_of("-D", "-I", "-U", "-L", "-l", 0))
		{
			// skip next word, being option argument
			i.remove(2);
		}
		else if (a.startswith_one_of("-D", "-U", "-I", "-l", "-L", 0))
		{
			// Something like "-DNDEBUG" or "-Wp,-MD,.deps/nsinstall.pp".  Just skip this word.
			i.remove();
		}
		else if (a.startswith_one_of("-Xmake-dependency", 0))
		{
			i.remove();
		}
    }
    
	args.trace("result");
}

//---------------------------------------------------------------------------------------------

// Remove "-o" options from argument list.
//
// This is used when running the preprocessor, when we just want it to write
// to stdout, which is the default when no -o option is specified.
//
// Structurally similar to dcc_strip_local_args()

void DiabCompiler::strip_dasho(Arguments &args)
{
	for (Arguments::Iterator i = args; !!i; ++i)
	{
		text a = *i;
		if (a == "-o")
			i.remove(2); // skip "-o  FILE"
        else if (a.startswith("-o"))
			i.remove(); // skip "-oFILE"
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
