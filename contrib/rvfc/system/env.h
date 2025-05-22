
#ifndef _rvfc_system_env_
#define _rvfc_system_env_

#include <string>

#include "rvfc/text/defs.h"

namespace rvfc
{

using std::string;
using namespace rvfc::Text;

///////////////////////////////////////////////////////////////////////////////////////////////

class Pathlet : public text
{
public:
	explicit Pathlet(const string &p)
	{
		if (p.find_first_not_of(" \t\n") == string::npos)
			return;
		int i = p.find_first_of(";");
		if (i != string::npos)
		{
			assign(p.substr(0, i));
			return;
		}
		assign(p);
		int j = 0;
		while (j < length() && (j = find_first_of("\\/", j)) != string::npos)
		{
			if (at(j) == '/')
				at(j) = '\\';
			++j;
			if (j < length() && (at(j) == '/' || at(j) == '\\'))
				erase(j, 1);
		}

		int n = length();
		j = find_last_not_of("\\/");
		if (j != string::npos && j+1 < length())
			erase(j+1);
	}

	bool operator!() const { return blank(); }
};

//---------------------------------------------------------------------------------------------

class EnvPath : public string
{
	static string fix(const string &s)
	{
		EnvPath p, p1;
		p.assign(s);
		while (!p.empty())
		{
			Pathlet q = p.pop_front();
			if (!!q)
				p1.push_back(q);
		}
		return p1;
	}

	static string trim(const string &s)
	{
		int i = s.find_first_not_of(" \t\n");
		int j = s.find_last_not_of(" \t\n");
		if (i == string::npos || j == string::npos)
			return "";
		return s.substr(i, j - i + 1);
	}

	static string tolower(const string &s)
	{
		string t = s;
		int n = t.length();
		for (int i = 0; i < n; ++i)
		{
			char &c = t.at(i);
			c = ::tolower(c);
		}
		return t; 
	}

	void assign(const string &s)
	{
		string::assign(s);
	}

public:
	EnvPath() {}

	EnvPath(const string &s) { assign(fix(s)); }
		
	EnvPath &push_front(const Pathlet &p)
	{
		if (!p)
			return *this;

		find_and_remove(p);
		if (empty())
			assign(p);
		else
			assign(p + ";" + *this);
		return *this;
	}

	EnvPath &push_back(const Pathlet &p)
	{
		if (!p || find(p) != string::npos)
			return *this;

		if (!empty())
			append(";");
		append(p);
		return *this;
	}

	EnvPath &push_front(const EnvPath &p)
	{
		EnvPath p1 = p;
		while (!p1.empty())
			push_front(p1.pop_back());
		return *this;
	}

	EnvPath &push_back(const EnvPath &p)
	{
		EnvPath p1 = p;
		while (!p1.empty())
			push_back(p1.pop_front());
		return *this;
	}

	EnvPath &operator<<=(const string &s) { return push_back(EnvPath(s)); }
	EnvPath &operator<<=(const EnvPath &p) { return push_back(p); }

	EnvPath operator<<(const string &s) const { return EnvPath(*this).push_back(EnvPath(s)); }
	EnvPath operator<<(const EnvPath &p) const { return EnvPath(*this).push_back(p); }

	EnvPath &operator>>=(const string &s) { return push_front(EnvPath(s)); }
	EnvPath &operator>>=(const EnvPath &p) { return push_front(p); }

	EnvPath operator>>(const string &s) const { return EnvPath(*this).push_front(EnvPath(s)); }
	EnvPath operator>>(const EnvPath &p) const { return EnvPath(*this).push_front(p); }

	Pathlet pop_front()
	{
		int i = find_first_of(";");
		if (i == string::npos)
		{
			Pathlet p(string(*this));
			assign("");
			return p;
		}
		Pathlet p(substr(0, i));
		assign(substr(i + 1, string::npos));
		return p;
	}

	Pathlet pop_back()
	{
		int i = find_last_of(";");
		if (i == string::npos)
		{
			Pathlet p(string(*this));
			assign("");
			return p;
		}
		Pathlet p(substr(i+1));
		assign(substr(0, i));
		return p;
	}

	int find(const Pathlet &p) const
	{
		int n = length();
		int m = p.length();
		int i = 0;
		while (i < n && i != string::npos)
		{
			int j = string::find(p, i);
			if (j == string::npos)
				return j;
			if (i > 0)
			{
				if (at(i - 1) != ';')
				{
					i = j + 1;
					continue;
				}
			}
			if (j + m == n || at(j + m) == ';')
				return j;
			i = find_first_of(';');
		}
		return string::npos;
	}

	bool find_and_remove(const Pathlet &p)
	{
		int i = find(p);
		if (i == string::npos)
			return false;
		erase(i, p.length());
		if (at(--i) == ';')
			erase(i, 1);
		return true;
	}

	bool operator!() const
	{
		return empty();
	}

	EnvPath filter(const string &t, bool out = false) const
	{
		string t1 = tolower(t);
		EnvPath p = *this, p1;
		while (!p.empty())
		{
			Pathlet q = p.pop_front();
			string q1 = tolower(q);
			bool f = q1.find(t1) != string::npos;
			if (out && !f || !out && f)
				p1.push_back(q);
		}
		return p1;
	}

	EnvPath filter_out(const string &t) const
	{
		return filter(t, true);
	}

	string to_str() const
	{
		string s;
		EnvPath p = *this;
		for (int i = 0; !!p; ++i)
		{
			Pathlet q = p.pop_front();
			s += stringf("[%02d] %s\n", i+1, +q);
		}
		return s;
	}
};

//---------------------------------------------------------------------------------------------

class SystemPath : public EnvPath
{
public:
	SystemPath() : EnvPath(getenv("PATH")) {}
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_system_env_
