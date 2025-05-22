/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*- 
 *
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool <mbp@samba.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


                /* "I have a bone to pick, and a few to break." */

/**
 * @file
 *
 * Functions for understanding and manipulating argument vectors.
 *
 * The few options explicitly handled by the client are processed in its
 * main().  At the moment, this is just --help and --version, so this function
 * never has to worry about them.
 *
 * We recognize two basic forms "distcc gcc ..." and "distcc ...", with no
 * explicit compiler name.  This second one is used if you have a Makefile
 * that can't manage two-word values for $CC; eventually it might support
 * putting a link to distcc on your path as 'gcc'.  We call this second one an
 * implicit compiler.
 *
 * We need to distinguish the two by working out whether the first argument
 * "looks like" a compiler name or not.  I think the two cases in which we
 * should assume it's implicit are "distcc -c hello.c" (starts with a hypen),
 * and "distcc hello.c" (starts with a source filename.)
 *
 * In the case of implicit compilation "distcc --help" will always give you
 * distcc's help, not gcc's, and similarly for --version.  I don't see much
 * that we can do about that.
 *
 * @todo We don't need to run the full argument scanner on the server, only
 * something simple to recognize input and output files.  That would perhaps
 * make the function simpler, and also mean that if argument recognizer bugs
 * are fixed in the future, they only need to be fixed on the client, not on
 * the server.  An even better solution is to have the client tell the server
 * where to put the input and output files.
 *
 * @todo Perhaps make the argument parser driven by a data table.  (Would that
 * actually be clearer?)  Perhaps use regexps to recognize strings.
 *
 * @todo We could also detect options like "-x cpp-output" or "-x
 * assembler-with-cpp", because they should override language detection based
 * on extension.  I haven't seen anyone use them yet though.  In fact, since
 * we don't assemble remotely it is moot for the only reported case, the
 * Darwin C library.  We would also need to update the option when passing it
 * to the server.
 *
 * @todo Perhaps assume that assembly code will not use both #include and
 * .include, and therefore if we preprocess locally we can distribute the
 * compilation?  Assembling is so cheap that it's not necessarily worth
 * distributing.
 **/


#include "common/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __linux__
#include <unistd.h>
#endif

#include <sys/stat.h>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/exitcode.h"
#include "common/snprintf.h"
#include "common/compiler.h"
#include "common/arg.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

Arguments::Arguments(int argc, char *argv[]) : rvfc::Arguments()
{
	rvfc::Arguments::append(argc, argv); // this calls distcc's Arguments::append()
}

//---------------------------------------------------------------------------------------------

void Arguments::append(const string &s)
{
	_args.push_back(s[0] != '/' ? s : string("-") + s.substr(1));
}

//---------------------------------------------------------------------------------------------

void Arguments::found_output_file(const string &file)
{
	rs_trace("found object/output file '%s'", file.c_str());
	if (!output_file.empty()) 
	{
		rs_log_info("error: numerous output files: '%s' and '%s'", output_file.c_str(), file.c_str());
		throw "MscCompiler::scan_args: error";
	}

	output_file = file;
}

//---------------------------------------------------------------------------------------------

bool Arguments::replace_file_args(const string &opt, const string &fname, int options)
{
	Iterator i = find(opt, false);
	if (!i)
		return false;

	if (*i == opt)
	{
		if (!(options & Arguments::Find_TwoOpt))
			return false;
		++i;
		if (!i)
			return false;
		*i = fname;
	}
	else
	{
		if (!(options & Arguments::Find_OneOpt))
			return false;
		*i = stringf("%s%s", opt.c_str(), fname.c_str());
	}
	return true;
}

//---------------------------------------------------------------------------------------------

void Arguments::trace(const char *message) const
{
    if (!rs_trace_enabled())
		return;

	text s = toString();
    rs_trace("%s: %s", message, +s);
}

//---------------------------------------------------------------------------------------------

// Change input file to @p ifname; called on compiler.
//
// @todo Unify this with dcc_scan_args
//
// @todo Test this by making sure that when the modified arguments are
// run through scan_args, the new ifname is identified as the input.

void Arguments::set_input(Compiler &compiler, const string &ifname)
{
	for (Arguments::Iterator i = *this; !!i; ++i) 
        if (compiler.is_source(*i))
		{
            rs_trace("changed input from \"%s\" to \"%s\"", i->c_str(), ifname.c_str());
            *i = ifname;
#ifdef _WIN32
            *i = convert_win_path(ifname);
#endif
            //dcc_trace_argv("command after", a);
            return;
        }
    
    rs_log_error("failed to find input file");
    throw "Arguments:set_input: failed to find input file";
}

//---------------------------------------------------------------------------------------------
// Some files should always be built locally...

bool Arguments::source_needs_local()
{
    text ext = input_file.ext();

    if (ext.startswith("conftest.") || ext.startswith("tmp.conftest.")) 
	{
        rs_trace("autoconf tests are run locally: %s", +input_file);
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------------------------

void dcc_note_compiled(const Path &input_file, const Path &output_file)
{
    rs_log(RS_LOG_INFO|RS_LOG_NONAME, "compile from %s to %s", +input_file.basename(), +output_file.basename());
}

///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

//---------------------------------------------------------------------------------------------

// This function converts unix style paths to windows style paths. 
// This is required because when cl sees a unix style path (starting with '/') it thinks it's
// an option (windows options start with '/') and promptly dies since options like /cygdrive 
// and /home don't exist in cl.
//
// The conversion happens inline.
//
// Note: We're using this rather than the cygpath code in order to be quicker (we don't have 
// to check the mount table or anything). 
//
// @arg oldname filename a pointer to the filename. 
// @arg offset how many characters from the begnning of the oldname until we have the actual
//      filename. Useful for cases like /Fofile.c
// @returns 0 if the filename is ok for cl. 1 if the file name is not ok (e.g. it's missing!).
//
// @todo fix memory leaks.

#define CYGWIN_LOCATION "c:/cygwin"

string Arguments::convert_win_path(const string &fname)
{
	return fname;
#if 0
	if (fname.empty())
	{
		rs_log_error("We got a bad parameter");
		throw "Arguments:convert_file_path: bad arguments";
	}

//	rs_log_info("converting file path \"%s\"", name );

	if (str_startswith(fname, "/cygdrive/"))
	{
		char * secondSlash = NULL;
		char driveLetter = '\0';

		// we need to offset by one to get  past the leading '/'
		secondSlash = strchr(name+1, (int)'/');
		driveLetter = *(secondSlash+1); 

		if ( (driveLetter > 'a' && driveLetter < 'z') || (driveLetter > 'A' && driveLetter < 'Z') )
		{
			// then our source is somewhere in the windows filesystem.
			// Windows drives can only be one letter according
			// to http://technet2.microsoft.com/WindowsServer/en/library/f7e61105-8033-4c45-a6d2-9476266aac221033.mspx?mfr=true 

			string windowsPath;
			int rc = 0;
			if (offset > 0)
			{
				// TODO: this is a mess! rewrite it so people can read it
				windowsPath = (char *) malloc(sizeof(char) * (offset + strlen(secondSlash+3) + 1 + 3));
				if (NULL ==  windowsPath)
				{
					// +3 is for the {driveletter}:\\
					// +1 is for the null byte that strlen doesn't count 
					// secondSlash+3 is where the real windows path begins


					rs_log_error("Convert to Windows Path Failed");
					throw "Arguments:convert_file_path: error";
				}
				memcpy(windowsPath, oldname[0][i], offset);
				windowsPath[offset]=driveLetter;
				memcpy(windowsPath+offset+1, ":/", 2); /* +1 to offset the drive letter too */
				strcpy(windowsPath+offset+3, secondSlash+3);

			}
			else
			{
				windowsPath = stringf("%c:/%s", driveLetter, secondSlash+3);
			}

			oldname[0][i] = windowsPath;
			rs_log_info("path is now: %s", oldname[0][i]);
			dcc_trace_argv("and full argv is:", oldname[0]);

			// FIXME stop leaking the old filepath

			return 0;
		}
	}

	if (*name == '/')
	{
		// then we have a unix style path that we have to convert to a Windows style path

		char * windowsPath = NULL;
		int rc = 0;

		if (offset > 0)
		{
			int cyg_loc_length = strlen(CYGWIN_LOCATION);
			// TODO: this is a mess! rewrite it so people can read it
			windowsPath = (char *) malloc(sizeof(char) * (cyg_loc_length + strlen(oldname[0][i])+ 1));
			if (NULL == windowsPath)
			{
				// +3 is for the {driveletter}:\\
				// +1 is for the null byte that strlen doesn't count
				// secondSlash+3 is where the real windows path begins

				rs_log_error("Convert to Windows Path Failed");
				throw "Arguments:convert_file_path: error";
			}

			memcpy(windowsPath, oldname[0][i], offset);
			strcpy(windowsPath+offset, CYGWIN_LOCATION);
			rs_log_info("Here's the windows path so far: '%s'", windowsPath);
			strcpy(windowsPath+offset+cyg_loc_length, (oldname[0][i] + offset) );
		}
		else 
		{
			windowsPath = stringf("%s%s", CYGWIN_LOCATION, name);
		}

		oldname[0][i] = windowsPath;
		rs_log_info("path is now: %s", oldname[0][i]);
		dcc_trace_argv("and full argv is:", oldname[0]);
		// FIXME stop leaking the old filepath
	}
#endif // 0
}

//---------------------------------------------------------------------------------------------

#endif // _WIN32

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
