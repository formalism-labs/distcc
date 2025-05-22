
#ifndef _rvfc_net_host_
#define _rvfc_net_host_

#if defined(_WIN32)

#elif defined(__linux__)
#endif

#include "rvfc/net/classes.h"
#include "rvfc/net/basics.h"

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////
// Hosts
///////////////////////////////////////////////////////////////////////////////////////////////

#define HOSTNAME_MAX_SIZE 255

class Host
{
	typedef Host This;

	friend class HostAndPort;

protected:
	Text::text _name;

public:
	explicit Host() {}
	explicit Host(const char *name);
	explicit Host(const IPAddress &addr);

	This &operator=(const char *name) { _name = name; return *this; }

	const std::string &name() const { return _name; }

	bool isAddress() const;
	IPAddress explicitAddress() const;

	bool isValidName() const;
	bool isValid() const { return isAddress() || isValidName(); }

	class Addresses
	{
		friend class Iterator;

		auto_free<struct addrinfo*> _info;

	public:
		Addresses(const char *node, int family = AF_UNSPEC);

		class Iterator
		{
			struct addrinfo *_info;

		public:
			Iterator(Addresses &addrs);

			Iterator &operator++();
			bool operator!() const;
			IPAddress operator*() const;
		};

		CLASS_EXCEPTION_DEF("rvfc::Host::Addresses");
		CLASS_SUB_EXCEPTION_DEF(InvalidErr, "invalid host specification"); 
	};

	virtual Addresses addresses() const;

	CLASS_EXCEPTION_DEF("rvfc::Host");
	CLASS_SUB_EXCEPTION_DEF(InvalidErr, "invalid host name"); 
};

//---------------------------------------------------------------------------------------------

class HostAndPort
{
	typedef HostAndPort This;

protected:
	void ctor(const char *str);
	bool set(const char *str, bool invalidate);

	Host _host;
	Port _port;
	bool _valid;

	void _invalidate() { _valid = false; }

public:
	explicit HostAndPort() : _host(), _port(0), _valid(true) {}
	HostAndPort(const Host &host, const Port &port) : _host(host), _port(port), _valid(true) {}
	HostAndPort(const char *str) { ctor(str); }
	HostAndPort(const std::string &str) { ctor(str.c_str()); }

	This &operator=(const char *name);
	bool set(const char *name);

	Text::text text() const;
	
	const Host host() const { return _host; }
	Host &host() { return _host; }
	
	const Port &port() const { return _port; }
	Port &port() { return _port; }

	bool isValid() const { return _valid && _host.isValid(); }

	CLASS_EXCEPTION_DEF("rvfc::HostAndPort");
	CLASS_SUB_EXCEPTION_DEF(InvalidErr, "invalid specification"); 
};

//---------------------------------------------------------------------------------------------

class Localhost : public Host
{
public:
	Localhost();

	virtual Addresses addresses() const;
};

//---------------------------------------------------------------------------------------------

// TBD
class LoopbackHost : public Host
{
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_net_host_
