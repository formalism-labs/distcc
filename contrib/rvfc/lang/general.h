
#ifndef _rvfc_lang_
#define _rvfc_lang_

#include <memory.h>

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////
// Preprocessor
///////////////////////////////////////////////////////////////////////////////////////////////

#define TO_STRING_(x) #x
#define TO_STRING(x) TO_STRING_(x) 

///////////////////////////////////////////////////////////////////////////////////////////////
// Numbers
///////////////////////////////////////////////////////////////////////////////////////////////

#define _1e3 1000L
#define _1e6 1000000L
#define _1e9 1000000000L

///////////////////////////////////////////////////////////////////////////////////////////////
// Null
///////////////////////////////////////////////////////////////////////////////////////////////

struct Null {};

const Null null = Null();

///////////////////////////////////////////////////////////////////////////////////////////////
// Object Sizes
///////////////////////////////////////////////////////////////////////////////////////////////

#define lengthof(v) (sizeof(v)/sizeof(*v))

//---------------------------------------------------------------------------------------------

template <class T>
int bitsCount()
{
	int n = 0;
	for (int i = 1; i; i <<= 1, ++n) ;
	return n;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Psuedo-formal Types
///////////////////////////////////////////////////////////////////////////////////////////////

#define enum__(Type) \
	struct Type \
	{ \
		enum _##Type {

#define __enum(Type) \
		} _x; \
	\
		typedef _##Type Enum; \
		explicit Type() {} \
		Type(_##Type x) : _x(x) {} \
		explicit Type(int k) : _x(_##Type(k)) {} \
	\
		Type &operator=(_##Type x) { _x = x; return *this; } \
		bool operator==(_##Type x) const { return _x == x; } \
		bool operator==(const Type &e) const { return _x == e._x; } \
		operator _##Type() const { return _x; } \
	} 

//---------------------------------------------------------------------------------------------

#define SCALAR_TYPE(Type, Name) \
	class Name \
	{ \
		Type _n; \
	\
	protected: \
		Name(Type n) : _n(n) {} \
	\
	public: \
		Name() : _n( 0 ) {} \
		Name(const Name &n) : _n(n._n) {} \
	\
		bool operator==(const Name &x) const { return _n == x._n; } \
	\
		Type &value() { return _n; } \
		const Type &value() const { return _n; } \
	\
		Name &operator=(const Name &n) { _n = n._n; return *this; } \
	}

//---------------------------------------------------------------------------------------------

#define VALUE_TYPE(V, T, v0) \
	class T \
	{ \
		V _val; \
	\
	public: \
		explicit T(V v = v0) : _val(v) {} \
	\
		V value() const { return _val; } \
	\
		T &operator=(const T &t) { _val = t._val; return *this; } \
	}

///////////////////////////////////////////////////////////////////////////////////////////////
// Delayed Construction
///////////////////////////////////////////////////////////////////////////////////////////////

#define NULL_CTOR_SPEC(T_) \
	class T_##_null_ctor \
	{ \
	protected: \
		T_##_null_ctor() {} \
	\
		friend T_;

#define NULL_CTOR_END }

#define FREE_NULL_CTOR_SPEC(T_) \
	class T_##_null_ctor {}

#define NULL_CTOR_DEF(T_) explicit T_(T_##_null_ctor)

#define DELAY_CTOR(T_, member) member(T_##_null_ctor())

#define DELAYED_CTOR(T_, member, args) \
	member.~T_(); \
	new (&member) T_ args

///////////////////////////////////////////////////////////////////////////////////////////////

class Memset0
{
public:
	Memset0(char *p, size_t n)
	{
		memset(p, 0, n);
	}
};

#define Memset0_CTOR Memset0((char *) this, sizeof(*this))

///////////////////////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
#define __thread __declspec(thread)
#endif

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#endif // _rvfc_lang_
