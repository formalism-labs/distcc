
#ifndef _distcc_common_arg_h_
#define _distcc_common_arg_h_

#include <list>
#include <vector>
#include <string>

#include "exitcode.h"

#include "rvfc/text/defs.h"
#include "rvfc/lang/defs.h"
#include "rvfc/filesys/defs.h"

namespace distcc
{

using std::string;
using namespace rvfc::Text;
using rvfc::Path;

class Compiler;

///////////////////////////////////////////////////////////////////////////////////////////////

class Arguments : public rvfc::Arguments
{
public:
	Path input_file;
	Path output_file;
	Path dotd_file;
	Path pdb_file;

	void found_output_file(const string &file);

public:
	Arguments() {}
	Arguments(const rvfc::Arguments &args) : rvfc::Arguments(args) {}
	Arguments(int argc, char *argv[]);

	void append(const string &s);

	enum
	{
		Find_OneOpt = 0x1,
		Find_TwoOpt = 0x2
	};
	bool replace_file_args(const string &opt, const string &fname, int options);

	static string convert_win_path(const string &fname);

	void trace(const char *message) const;

	void set_input(Compiler &compiler, const string &ifname);

	bool source_needs_local();
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif // _distcc_common_arg_h_
