
#ifndef _rvfc_text_text_
#define _rvfc_text_text_

#include <stdexcept>
#include <string>
#include <regex>

#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

namespace rvfc
{
namespace Text
{

using std::string;

///////////////////////////////////////////////////////////////////////////////////////////////

class text : public string
{
public:
	text() {}
	text(const string &s) : string(s) {}
	text(const std::tr1::smatch &m) : string(m.str()) {}
	text(const std::tr1::ssub_match &m) : string(m.str()) {}
	text(const char *cp) : string(cp) {}
	text(const char *cp, int n) : string(cp, n) {}
//	text(long n) : string(std::tr1::to_string()) {}

	bool startwith(const text &s) const{ return find(s) == 0; }

	text ltrim() const;
	text rtrim() const;
	text trim() const { return rtrim().ltrim(); }

	long to_int(const char *what = 0) const;
	double to_num(const char *what = 0) const;

	text tolower() const;
	text toupper() const;

	bool blank() const { return empty() || trim().empty(); }
	bool operator!() const { return blank(); }

	text operator()(size_t pos, size_t count = string::npos) const { return substr(pos, count); }

	bool contains(const string &s) const;
	bool startswith(const string &s) const;
	bool endswith(const string &s) const;

	bool equalsto_one_of(const char *p, ...) const;
	bool startswith_one_of(const char *p, ...) const;
	bool endswith_one_of(const char *p, ...) const;

	//-----------------------------------------------------------------------------------------

	class regex
	{
	public:
		typedef std::tr1::smatch match;

		class iterator : public std::tr1::sregex_iterator
		{
			std::tr1::regex *_re;

		public:
			iterator(const text &text, std::tr1::regex *re) : std::tr1::sregex_iterator(text.begin(), text.end(), *re), _re(re) {}
			~iterator();

			bool operator!() const { return *this == std::tr1::sregex_iterator(); }
			const char *c_str() const { return operator*().str().c_str(); }
			std::tr1::smatch match(int i) const { std::tr1::smatch m = **this; return m; }
			std::tr1::smatch operator[](int i) const { return match(i); }

		};
	};

	bool search(const char *re, std::tr1::smatch &match) const;
	bool match(const char *re, std::tr1::smatch &match) const;
	regex::iterator scan(const char *re) const;
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Text
} // namespace rvfc

#endif // _rvfc_text_text_
