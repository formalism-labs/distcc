/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003 by Martin Pool <mbp@samba.org>
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

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <list>
#include <iostream>
#include <fstream>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "exitcode.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////
     
/**
 * Load a whole file into a new string in a malloc'd memory buffer.
 *
 * Files larger than a certain reasonableness limit are not loaded, because
 * this is only used for reasonably short text files.
 *
 * Files that do not exist cause EXIT_NO_SUCH_FILE, but no error message.
 * (This suits our case of loading configuration files.  It could be made
 * optional.)
 **/

class TextFile
{
	fd_t _fd;
	string _fname;

public:
	TextFile(const string &fname) : _fname(fname)
	{
		_fd.socket = 0;
		_fd.fd = open(fname.c_str(), O_RDONLY);
		if (_fd.fd == -1) 
		{
			if (errno == EEXIST)
				throw "dcc_load_file_string: file does not exist";

			rs_log_warning("failed to open %s: %s", fname.c_str(), strerror(errno));
			throw "dcc_load_file_string: cannot open";
		}
	}

	~TextFile()
	{
		dcc_close(_fd);
	}

	string read()
	{
		// Find out how big the file is
		struct stat sb;
		if (fstat(_fd.fd, &sb) == -1) 
		{
			rs_log_error("fstat %s failed: %s", _fname.c_str(), strerror(errno));
			dcc_close(_fd);
			throw "File: cannot determine size";
		}

		if (sb.st_size > 1<<20) 
		{
			rs_log_error("%s is too large to load (%ld bytes)", _fname.c_str(), (long) sb.st_size);
			throw "File: too large to load";
		}

		// Allocate a buffer, allowing space for a nul.
		string s;
		s.resize(sb.st_size);

		// Read everything
		ssize_t read_bytes = ::read(_fd.fd, (char *) s.c_str(), (size_t) sb.st_size);
		if (read_bytes == -1) 
		{
			rs_log_error("failed to read %s: %s", _fname.c_str(), strerror(errno));
			throw "File: failed to read";
		}

		return s;
	}

	// skip comments and blank lines
	list<string> read_lines()
	{

	}
};

class TextLinesFile
{
	list<string> _lines;

public:
	TextLinesFile(const File &fname)
	{
		std::ifstream file(+fname.path());
		if (!file.is_open())
			throw "TextLinesFile: cannot open file";

		text line;
		while (getline(file, line))
		{
			if (!line.startwith("#"))
				_lines.push_back(line);
		}
		file.close();
	}

	list<string> lines() { return _lines; }
};

string dcc_load_file_string(const string &filename)
{
	return TextFile(filename).read();
}

list<string> dcc_load_file_lines(const File &filename)
{
	return TextLinesFile(filename).lines();
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
