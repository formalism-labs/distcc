
#ifndef _rvfc_types_set_
#define _rvfc_types_set_

#include <stdexcept>
#include <algorithm> 
#include <functional> 
#include <set>
#include <string>

namespace rvfc
{

using std::set;
using std::string;

///////////////////////////////////////////////////////////////////////////////////////////////

template <class T = std::string>
class Set : public std::set<T>
{
	typedef Set<T> This;

public:
	This &operator<<(const T &t) { insert(t); return *this; }
	bool in(const T &t) const { return find(t) != end(); }
	bool operator[](const T &t) const { return in(t); }
	bool operator!() const { return empty(); }

	class Iterator : public std::set<T>::const_iterator
	{
		const Set<T> &_set;

	public:
		Iterator(const Set &set) : std::set<T>::const_iterator(set.begin()), _set(set) {}
		Iterator(const Iterator &i) : std::set<T>::const_iterator(i), _set(i.set) {}

		bool operator!() const { return *this == _set.end(); }
	};
};


typedef Set<string> Sext;

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_types_set_
