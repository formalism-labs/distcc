
#include "uuid.h"

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////

void Uuid::assert_status_ok(RPC_STATUS &rc)
{
	if (rc != RPC_S_OK)
		throw std::runtime_error("UUID operation failed");
}

//----------------------------------------------------------------------------------------------

int Uuid::compare(const Uuid &id) const
{
	RPC_STATUS rc;
	int result = UuidCompare((UUID *) &_uuid, (UUID *) &id._uuid, &rc);
	assert_status_ok(rc);
	return result;
}

bool Uuid::operator==(const Uuid &id) const { return compare(id) == 0; }
bool Uuid::operator>(const Uuid &id) const { return compare(id) == 1; }
bool Uuid::operator<(const Uuid &id) const { return compare(id) == -1; }

//----------------------------------------------------------------------------------------------

bool Uuid::operator!() const
{
	RPC_STATUS rc;
	int result = UuidIsNil((UUID *) &_uuid, &rc);
	assert_status_ok(rc);
	return !!result;
}

//----------------------------------------------------------------------------------------------

Uuid::operator std::string() const
{
	unsigned char *pc;
	RPC_STATUS rc = UuidToString((UUID *) &_uuid, &pc);
	assert_status_ok(rc);
	std::string s = (char *) pc;
	RpcStringFree(&pc);
	return s;
}

//----------------------------------------------------------------------------------------------

unsigned short Uuid::hash() const
{
	RPC_STATUS rc;
	int result = UuidHash((UUID *) &_uuid, &rc);
	assert_status_ok(rc);
	return result;
}

//----------------------------------------------------------------------------------------------

Uuid::Uuid()
{
	RPC_STATUS rc = UuidCreate(&_uuid);
	assert_status_ok(rc);
}

//----------------------------------------------------------------------------------------------

Uuid::Uuid(const std::string &s)
{
	RPC_STATUS rc = UuidFromString((unsigned char *) s.c_str(), &_uuid);
	assert_status_ok(rc);
}

//----------------------------------------------------------------------------------------------

NullUuid Uuid::null;

///////////////////////////////////////////////////////////////////////////////////////////////

NullUuid::NullUuid()
{
	RPC_STATUS rc = UuidCreateNil(&_uuid);
	assert_status_ok(rc);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace green
