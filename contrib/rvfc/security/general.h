
#ifndef _rvfc_security_general_
#define _rvfc_security_general_

#include "rvfc/classes.h"

#include "rvfc/lang/general.h"
#include "rvfc/net/block.h"

#ifndef _WIN32
#define OPENSSL_INC(f) TO_STRING(openssl/f)
#else
#define OPENSSL_INC(f) TO_STRING(contrib/open-ssl/0.9.8/windows/openssl/f)
#endif

#include OPENSSL_INC(ssl.h)
#include OPENSSL_INC(err.h)

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////

class OpenSSL
{
public:
	OpenSSL();
	~OpenSSL();
};

//---------------------------------------------------------------------------------------------

#define THROW_SSL(E) \
	throw (const E &) rvfc::Exception::Adapter<E>(E(__FILE__, __LINE__, \
		rvfc::Exception::SSLError()))

#define ERROR_SSL(E) \
	_invalidate(), rvfc::LastException::Proxy() = (const E &) \
		rvfc::Exception::Adapter<E, rvfc::SSLError>( \
			E(__FILE__, __LINE__), \
			rvfc::SSLError())

class SSLError : public rvfc::Exception::SpecificError
{
public:
	SSLError();
	std::string text() const;
};

///////////////////////////////////////////////////////////////////////////////////////////////

namespace _BIO
{

//---------------------------------------------------------------------------------------------

class Bio
{
protected:
	BIO *_bio;

public:
	Bio() : _bio(0) {}
	~Bio();

	operator BIO*() { return _bio; }
	operator const BIO*() const { return _bio; }
};

//---------------------------------------------------------------------------------------------

class File : public Bio
{
public:
	File(const char *name);

	CLASS_EXCEPTION_DEF("rvfc::BIO::File");
};

//---------------------------------------------------------------------------------------------

class Memory : public Bio
{
public:
	Memory();

	Block block() const;

	CLASS_EXCEPTION_DEF("rvfc::BIO::Memory");
};

//---------------------------------------------------------------------------------------------

} // namespace _BIO

///////////////////////////////////////////////////////////////////////////////////////////////

class Certificate
{
	X509 *_cert;

public:
	Certificate(const X509 *x509);
	Certificate(const Block &block);
	Certificate(const char *filename);
	~Certificate();

	operator const X509*() const { return _cert; }
	operator X509*() { return _cert; }

	//-----------------------------------------------------------------------------------------
	// Name
	//-----------------------------------------------------------------------------------------
	
	class Name
	{
		const X509_NAME *_name;

	public:
		Name(const X509_NAME *name) : _name(name) {}

		TCString<256> oneLiner() const
		{
			char s[256];
			X509_NAME_oneline((X509_NAME *) _name, s, sizeof(s));
			return s;
		}

		const X509_NAME *name() const { return _name; }
		operator const X509_NAME*() const { return name(); }
	};
	
	//-----------------------------------------------------------------------------------------

	Name subjectName() const
	{
		return X509_get_subject_name((X509 *) _cert);
	}

	TCString<256> peerCommonName() const
	{
		char common_name[256];
		X509_NAME_get_text_by_NID(const_cast<X509_NAME *>(subjectName().name()), NID_commonName, 
			common_name, sizeof(common_name));
		return common_name;
	}

	std::string text() const;

	//-----------------------------------------------------------------------------------------

	CLASS_EXCEPTION_DEF("rvfc::Certificate");
};

//---------------------------------------------------------------------------------------------

class CACertificate : public Certificate
{
public:
	CACertificate(const X509 *x509) : Certificate(x509) {}
	CACertificate(const Block &block) : Certificate(block) {}
	CACertificate(const char *filename) : Certificate(filename) {}
};

//---------------------------------------------------------------------------------------------

class PrivateKey
{
	EVP_PKEY *_pkey;

public:
	PrivateKey(const char *filename);
	PrivateKey(const PrivateKey &key);
	~PrivateKey();

	operator const EVP_PKEY*() const { return _pkey; }
	operator EVP_PKEY*() { return _pkey; }

	bool operator==(const PrivateKey &key) const;
	bool operator!=(const PrivateKey &key) const { return !operator==(key); }

	CLASS_EXCEPTION_DEF("rvfc::PrivateKey");
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_security_general_
