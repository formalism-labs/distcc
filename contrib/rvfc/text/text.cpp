
#include "text/text.h"
#include "text/string.h"

namespace rvfc
{
namespace Text
{

using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////

text text::ltrim() const
{
	string s = *this;
	s.erase(s.begin(), find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

//---------------------------------------------------------------------------------------------

text text::rtrim() const
{
	string s = *this;
	s.erase(find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

///////////////////////////////////////////////////////////////////////////////////////////////

long text::to_int(const char *what) const
{
	long n;
	int k = sscanf(c_str(), "%ld", &n);
	if (!k)
		if (what)
			throw std::invalid_argument(stringf("%s: cannot convert '%s' to integer", what, c_str()));
		else
			throw std::invalid_argument(stringf("cannot convert '%s' to integer", c_str()));
	return n;
}

//---------------------------------------------------------------------------------------------

double text::to_num(const char *what) const
{
	double n;
	int k = sscanf(c_str(), "%f", &n);
	if (!k)
	{
		if (what)
			throw std::invalid_argument(stringf("%s: cannot convert '%s' to number", what, c_str()));
		else
			throw std::invalid_argument(stringf("cannot convert '%s' to number", c_str()));
	}
	return n;
}

///////////////////////////////////////////////////////////////////////////////////////////////

text text::tolower() const
{
	text t = *this;
	int n = t.length();
	for (int i = 0; i < n; ++i)
	{
		char &c = t.at(i);
		c = ::tolower(c);
	}
	return t; 
}

//---------------------------------------------------------------------------------------------

text text::toupper() const
{
	text t = *this;
	int n = t.length();
	for (int i = 0; i < n; ++i)
	{
		char &c = t.at(i);
		c = ::toupper(c);
	}
	return t; 
}

///////////////////////////////////////////////////////////////////////////////////////////////

bool text::contains(const string &s) const
{
	return find(s) != string::npos;
}

//---------------------------------------------------------------------------------------------

bool text::startswith(const string &s) const
{
	return find(s) == 0;
}

//---------------------------------------------------------------------------------------------

bool text::endswith(const string &s) const
{
	size_t i = rfind(s);
	return i != string::npos && i == length() - s.length();
}

//---------------------------------------------------------------------------------------------

bool text::equalsto_one_of(const char *p, ...) const
{
	bool rc = false;
	va_list args;
	va_start(args, p);
	for (;;)
	{
		if (!p)
			break;
		if (*this == p)
		{
			rc = true;
			break;
		}
		p = va_arg(args, char*);
	}
	va_end(args);
	return rc;
}

//---------------------------------------------------------------------------------------------

bool text::startswith_one_of(const char *p, ...) const
{
	bool rc = false;
	va_list args;
	va_start(args, p);
	for (;;)
	{
		if (!p)
			break;
		if (startswith(p))
		{
			rc = true;
			break;
		}
		p = va_arg(args, char*);
	}
	va_end(args);
	return rc;
}

//---------------------------------------------------------------------------------------------

bool text::endswith_one_of(const char *p, ...) const
{
	bool rc = false;
	va_list args;
	va_start(args, p);
	for (;;)
	{
		if (!p)
			break;
		if (startswith(p))
		{
			rc = true;
			break;
		}
		p = va_arg(args, char*);
	}

	va_end(args);
	return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////

bool text::search(const char *re_p, tr1::smatch &match) const
{
	tr1::regex re(re_p);
	bool b = regex_search(*this, match, re);
	return b;
}

//---------------------------------------------------------------------------------------------

bool text::match(const char *re_p, tr1::smatch &match) const
{
	tr1::regex re(re_p);
	bool b = regex_match(*this, match, re);
	return b;
}

//---------------------------------------------------------------------------------------------

text::regex::iterator text::scan(const char *re) const
{
	tr1::regex *pre = new tr1::regex(re);
	return text::regex::iterator(*this, pre);
}

//---------------------------------------------------------------------------------------------

text::regex::iterator::~iterator()
{
	delete _re;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Text
} // namespace rvfc
