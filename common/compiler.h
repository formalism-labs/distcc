/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2006 by Tom Aratyn (themystic.ca@gmail.com)
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


#ifndef _DCC_COMPILER_H__
#define _DCC_COMPILER_H__

#include "arg.h"

#include "rvfc/filesys/defs.h"

namespace distcc
{

using rvfc::File;

///////////////////////////////////////////////////////////////////////////////////////////////

class Compiler
{
public:
	Compiler()
	{
		strcpy(preproc, "-E");
	}

    // The compiler option to specify you want preprocessed file. eg. -E /P.
    char preproc[8];

	virtual void scan_args(Arguments &args, bool on_server) = 0;

	// For comapatibility reasons, please use dcc_parse_hosts_file() when writing your own version of this
//	int (*get_hostlist)(dcc_hostdef **ret_list, int *ret_nhosts);
	virtual string preproc_exten(const string &e) const = 0;

	virtual bool is_source(const string &sfile) const = 0;
	virtual bool is_object(const string &sfile) const = 0;
	virtual bool is_preprocessed(const string &sfile) const = 0;

	virtual void strip_dasho(Arguments &args) = 0;
	virtual void strip_local_args(Arguments &args, bool on_server) = 0;
	virtual int set_action_opt(Arguments &args, const string &new_c) = 0;
	virtual int set_output(Arguments &args, const Path &o_fname, const Path &dotd_fname, const Path &pdb_fname) = 0;

	void set_input(Arguments &args, const string &i_fname) { args.set_input(*this, i_fname); }
};

//---------------------------------------------------------------------------------------------

class MscCompiler : public Compiler
{
public:
    void scan_args(Arguments &args, bool on_server);

	string preproc_exten(const string &e) const;

	bool is_source(const string &sfile) const;
	bool is_object(const string &sfile) const;
	bool is_preprocessed(const string &sfile) const;

	void strip_dasho(Arguments &args);
	void strip_local_args(Arguments &args, bool on_server);
	int set_action_opt(Arguments &args, const string &new_c);
	int set_output(Arguments &args, const Path &o_fname, const Path &dotd_fname, const Path &pdb_fname);
};

//---------------------------------------------------------------------------------------------

class GccCompiler : public Compiler
{
public:
    void scan_args(Arguments &args, bool on_server);

	string preproc_exten(const string &e) const;

	bool is_source(const string &sfile) const;
	bool is_object(const string &sfile) const;
	bool is_preprocessed(const string &sfile) const;

	void strip_dasho(Arguments &args);
	void strip_local_args(Arguments &args, bool on_server);
	int set_action_opt(Arguments &args, const string &new_c);
	int set_output(Arguments &args, const Path &o_fname, const Path &dotd_fname, const Path &pdb_fname);
};

//---------------------------------------------------------------------------------------------

class DiabCompiler : public Compiler
{
public:
    void scan_args(Arguments &args, bool on_server);

	string preproc_exten(const string &e) const;

	bool is_source(const string &sfile) const;
	bool is_object(const string &sfile) const;
	bool is_preprocessed(const string &sfile) const;

	void strip_dasho(Arguments &args);
	void strip_local_args(Arguments &args, bool on_server);
	int set_action_opt(Arguments &args, const string &new_c);
	int set_output(Arguments &args, const Path &o_fname, const Path &dotd_fname, const Path &pdb_fname);
};

//---------------------------------------------------------------------------------------------

extern Compiler *dcc_compiler;

// Check which compiler is being used and sets the compiler specific functions inside of dcc_compiler
void dcc_set_compiler(const Arguments &args, int first_arg = 0);

int fix_dotd_file(const File &temp_d_fname, const File &temp_o_fname);

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif // _DCC_COMPILER_H__
