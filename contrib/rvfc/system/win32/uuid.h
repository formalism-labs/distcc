
#ifndef _rvfc_system_win32_uuid_
#define _rvfc_system_win32_uuid_

#include <rpc.h>
#include <string>

#pragma comment(lib, "rpcrt4")

namespace rvfc
{

class NullUuid;

///////////////////////////////////////////////////////////////////////////////////////////////

class Uuid
{
protected:
	UUID _uuid;
	UUID &uuid() { return _uuid; }

	int compare(const Uuid &id) const;

	static void assert_status_ok(RPC_STATUS &rc);

public:
	Uuid();
	Uuid(const std::string &s);

	bool operator==(const Uuid &id) const;
	bool operator>(const Uuid &id) const;
	bool operator<(const Uuid &id) const;
	bool operator!() const;

	operator std::string() const;
	unsigned short hash() const;

	static NullUuid null;
};

//---------------------------------------------------------------------------------------------

class NullUuid : public Uuid
{
public:
	NullUuid();
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace namespace rvfc

#endif // _rvfc_system_win32_uuid_
