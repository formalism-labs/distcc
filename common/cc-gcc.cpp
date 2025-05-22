
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
// GCC stuff
///////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Apple extensions:
 * file.mm, file.M 
 * Objective-C++ source code which must be preprocessed. (APPLE ONLY) 
 *
 * file.mii Objective-C++ source code which should not be preprocessed. (APPLE ONLY)
 *
 * http://developer.apple.com/techpubs/macosx/DeveloperTools/gcc3/gcc/Overall-Options.html
 */
    
//---------------------------------------------------------------------------------------------

// If you preprocessed a file with extension @p e, what would you get?
//
// @param e original extension (e.g. ".c")
// @returns preprocessed extension, (e.g. ".i"), or NULL if unrecognized.

string GccCompiler::preproc_exten(const string &ext) const
{
	text e = ext;
    if (e[0] != '.')
        return "";

	if (e.equalsto_one_of(".i", ".c", 0)) 
        return ".i";

	if (e.equalsto_one_of(".c", ".cc", ".cpp", ".cxx", ".cp", ".c++", ".C", ".ii", 0))
        return ".ii";

	if (e.equalsto_one_of(".mi", ".m", 0)) 
        return ".mi";

	if (e.equalsto_one_of(".mii", ".mm", ".M", 0)) 
        return ".mii";

	if (e.equalsto_one_of(".s", ".S", 0)) 
        return ".s";

	return "";
}

//---------------------------------------------------------------------------------------------

// Does the extension of this file indicate that it is already preprocessed?

bool GccCompiler::is_preprocessed(const string &sfile) const
{
    text ext = dcc_find_extension(sfile);
    if (!ext)
        return false;

	switch (ext[1]) 
	{
#ifdef ENABLE_REMOTE_ASSEMBLE
    case 's':
        // .S needs to be run through cpp; .s does not
        return ext == ".s";
#endif
    case 'i':
        return ext.equalsto_one_of(".i", ".ii", 0);

    case 'm':
		return ext.equalsto_one_of(".mi", ".mii", 0);

    default:
        return false;
    }
}

//---------------------------------------------------------------------------------------------
// Work out whether @p sfile is source based on extension

bool GccCompiler::is_source(const string &sfile) const
{
    text ext = dcc_find_extension(sfile);
    if (!ext)
        return false;

	// you could expand this out further into a RE-like set of case
	// statements, but i'm not sure it's that important.

    switch (ext[1]) 
	{
    case 'i':
        return ext.equalsto_one_of(".i", ".ii", 0);

    case 'c':
        return ext.equalsto_one_of(".c", ".cc", ".cpp", ".cxx", ".cp", ".c++", 0);

	case 'C':
        return ext == ".C";

	case 'm':
        return ext.equalsto_one_of(".m", ".mm", ".mi", ".mii", 0);

	case 'M':
        return ext == ".M";

#ifdef ENABLE_REMOTE_ASSEMBLE
    case 's':
        return ext == ".s";

    case 'S':
        return ext == ".S";
#endif

	default:
        return false;
    }
}

//---------------------------------------------------------------------------------------------
// Decide whether @p filename is an object file, based on its extension

bool GccCompiler::is_object(const string &sfile) const
{
	return dcc_find_extension(sfile) == ".o";
}

///////////////////////////////////////////////////////////////////////////////////////////////
// GCC stuff
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

void GccCompiler::scan_args(Arguments &args, bool on_server)
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
			else if (a.equalsto_one_of("-MD", "-MMD", 0)) 
			{
                // These two generate dependencies as a side effect. They should work with the way we call cpp.
            }
			else if (a.equalsto_one_of("-MG", "-MP", 0)) 
			{
                // These just modify the behavior of other -M* options and do nothing by themselves
            }
			else if (a == "-MF")
			{
                // as above but with extra argument
				args.dotd_file = *++i;
            }
			else if (a.equalsto_one_of("-MT", "-MQ", 0)) 
			{
                // as above but with extra argument
                ++i;
            }
			else if (a[1] == 'M') 
			{
                // -M(anything else) causes the preprocessor to produce a list of make-style 
				// dependencies on header files, either to stdout or to a local file.
                // It implies -E, so only the preprocessor is run, not the compiler.  
				// There would be no point trying to distribute it even if we could.
                rs_trace("%s implies -E (maybe) and must be local", +a);
                throw "GccCompiler::scan_args: error";
            }
			else if (a.startswith("-Wa,")) 
			{
                // Look for assembler options that would produce output files and must be local.
                // Writing listings to stdout could be supported but it might be hard to parse reliably.
				if (a.contains(",-a") || a.contains("--MD"))
				{
                    rs_trace("%s must be local", +a);
                    throw "GccCompiler::scan_args: error";
                }
            } 
			else if (a.startswith("-specs=")) 
			{
                rs_trace("%s must be local", +a);
                throw "GccCompiler::scan_args: error";
            } 
			else if (a == "-S")
			{
                seen_opt_s = true;
            }
			else if (a == "-fprofile-arcs" || a == "-ftest-coverage") 
			{
                rs_log_info("compiler will emit profile info; must be local");
                throw "GccCompiler::scan_args: error";
            }
			else if (a == "-frepo")
			{
                rs_log_info("compiler will emit .rpo files; must be local");
                throw "GccCompiler::scan_args: error";
            }
			else if (a.startswith("-x")) 
			{
                rs_log_info("gcc's -x handling is complex; running locally");
                throw "GccCompiler::scan_args: error";
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
                    throw "GccCompiler::scan_args: error";
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
        throw "GccCompiler::scan_args: error";
    }

    if (!args.input_file)
	{
        rs_log_info("no visible input file");
        throw "GccCompiler::scan_args: error";
    }

    if (args.source_needs_local())
        throw "GccCompiler::scan_args: error";

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
                throw "GccCompiler::scan_args: error";
        }
		else if (seen_opt_c) 
		{
            if (dcc_output_from_source(args.input_file, ".o", ofile))
                throw "GccCompiler::scan_args: error";
        }
		else 
		{
            rs_log_crit("this can't be happening(%d)!", __LINE__);
            throw "GccCompiler::scan_args: error";
        }

        rs_log_info("no visible output file, going to add '-o %s' at end", +ofile);
		args << "-o" << ofile;
        args.output_file = ofile;
    }

    dcc_note_compiled(args.input_file, args.output_file);

    if (args.output_file == "-") 
	{
        // Different compilers may treat "-o -" as either "write to stdout", or "write to a file called '-'". 
		// We can't know, so we just always run it locally.  Hopefully this is a pretty rare case.
        rs_log_info("output to stdout?  running locally");
		throw "GccCompiler::scan_args: error";
    }
}

//---------------------------------------------------------------------------------------------
// Used to change "-c" or "-S" to "-E", so that we get preprocessed source.

int GccCompiler::set_action_opt(Arguments &args, const string &new_c)
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

int GccCompiler::set_output(Arguments &args, const Path &o_fname, const Path &dotd_fname, const Path &pdb_fname)
{
	args.trace("command before");

	Arguments::Iterator i = args.find("-o");
	string orig_o_fname = !!i ? i[1] : "";

	if (!args.replace_file_args("-o", o_fname, Arguments::Find_OneOpt | Arguments::Find_TwoOpt))
	{
		rs_log_error("failed to find '-o'");
		return EXIT_DISTCC_FAILED;
	}

	args.replace_file_args("-MF", dotd_fname, Arguments::Find_OneOpt | Arguments::Find_TwoOpt);

	args << "-MT" << orig_o_fname;

	args.trace("command after");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// GCC stuff
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

void GccCompiler::strip_local_args(Arguments &args, bool on_server)
{
	if (!on_server)
		return;

    // skip through argv, copying all arguments but skipping ones that ought to be omitted
	for (Arguments::Iterator i = args; !!i; ++i)
	{
		text a = *i;

		if (a.equalsto_one_of(
			"-D", "-I", "-U", "-L", "-l", "-MF", "-MT", "-MQ", "-include", "-imacros", 
			"-iprefix", "-iwithprefix", "-isystem", "-iwithprefixbefore", "-idirafter", 0))
		{
			// skip next word, being option argument
			i.remove(2);
		}
		else if (a.startswith_one_of("-Wp,", "-Wl,", "-D", "-U", "-I", "-l", "-L", 0))
		{
			// Something like "-DNDEBUG" or "-Wp,-MD,.deps/nsinstall.pp".  Just skip this word.
			i.remove();
		}
		else if (a.equalsto_one_of("-undef", "-nostdinc", "-nostdinc++", "-MD", "-MMD", "-MG", "-MP", 0))
		{
			// Options that only affect cpp; skip
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

void GccCompiler::strip_dasho(Arguments &args)
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
