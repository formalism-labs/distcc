
#ifndef _distcc_server_server_h_
#define _distcc_server_server_h_

#include <string>
#include <list>

#include "common/distcc.h"
#include "common/arg.h"

#include "rvfc/defs.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

class Server
{
public:
	Server();
	~Server();

	int run(const Arguments &args);

};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif // _distcc_server_server_h_  
