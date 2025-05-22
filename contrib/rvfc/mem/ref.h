
///////////////////////////////////////////////////////////////////////////////////////////////
// Smart Pointers
///////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _rvfc_memory_ref_
#define _rvfc_memory_ref_

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4786 4348)
#endif

#include "rvfc/mem/classes.h"

#include "rvfc/lang/general.h"
#include "rvfc/exceptions/general.h"
#include "rvfc/sync/atomic.h"

namespace rvfc
{

///////////////////////////////////////////////////////////////////////////////////////////////

class Object
{
public:
	Object() : _ref(0) {}
	Object(const Object &o) {}
	virtual ~Object() {}

	AtomicUINT32 _ref;

	int incref() { return ++_ref; }

	virtual unsigned long decref()
	{
		if (!--_ref)
			delete this;
		return _ref; 
	}

	Object *deref() { incref(); return this; }
};

#define isan_Object virtual public rvfc::Object

//---------------------------------------------------------------------------------------------

template <class T>
class GeneralRef
{
	typedef GeneralRef<T> This;

	friend class ref<T>;
	friend class const_ref<T>;

	template <class T1> friend class GeneralRef;

protected:
	GeneralRef(T *obj = 0) : _obj(obj)
	{
		if (obj)
			obj->deref();
	}

	GeneralRef(const ref<T> &r) : _obj(r.deref_object()) {}
	GeneralRef(const const_ref<T> &r) : _obj(r.deref_object()) {}
/*	
	template <class T1>
	GeneralRef(const ref<T1> &r) : _obj(r.deref_object()) {}

	template <class T1>
	GeneralRef(const ptr<T1> &p) : _obj(p.deref_object()) {}
*/
	template <class T1>
	GeneralRef(const GeneralRef<T1> &r)
	{
		_obj = dynamic_cast<T*>(r.deref_object());
		if (!_obj)
			THROW_EX(Error)("invalid cast");
	}

	virtual ~GeneralRef() { cleanup(); }

	mutable T *_obj;

	T &object() const 
	{
		if (!_obj)
			THROW_EX(Error)("access of null reference");
		return *_obj; 
	}

    T *deref_object() const 
	{ 
		if (!_obj)
			return 0;
		object().deref();
		return _obj;
	}

	void cleanup()
	{
		if (_obj)
			_obj->decref();
		_obj = 0;
	}

	void assign(T *data)
	{
		cleanup();
		if (!data)
			THROW_EX(Error)("null pointer assignment");
		data->incref();
		_obj = data;
	}

	void assign(const This &ref)
	{
		if (&ref == this || ref._obj == _obj)
			return;
		cleanup();
		T *p = ref.deref_object();
		if (!p)
			THROW_EX(Error)("null pointer assignment");
		_obj = p;
	}

	UINT32 refs() const { return object()._ref; }

	CLASS_EXCEPTION_DEF("rvfc::GeneralRef"); 
};

//---------------------------------------------------------------------------------------------

template <class T>
class ref : public GeneralRef<T>
{
	friend class const_ref<T>;

	typedef GeneralRef<T> Super;
	typedef ref<T> This;

public:
	typedef const_ref<T> Const;

public:
	ref() {}
	ref(T *data) : Super(data) {}
	ref(const This &ref) : Super(ref) {}
	ref(const ptr<T> &p) : Super(p) {}
	ref(const GeneralRef<Object> &r) : Super(r) {}

	template <class T1>
	explicit ref(const ref<T1> &r) : Super(r) {}

	This &operator=(T *data) { assign(data); return *this; }
	This &operator=(const This &ref) { assign(ref); return *this; }

	operator T&() { return object(); }
	operator const T&() const { return object(); }
	operator T*() { return &object(); }
	operator const T*() const { return &object(); }
	operator ref<Object>() const { return &object(); }

	T *operator->() { return &object(); }
	const T *operator->() const { return &object(); }
	T &operator*() { return object(); }
	const T &operator*() const { return object(); }

protected:
	T &object() const { return Super::object(); }
};

//---------------------------------------------------------------------------------------------

template <class T>
class const_ref : public GeneralRef<T>
{
	typedef GeneralRef<T> Super;
	typedef const_ref<T> This;

public:
	const_ref() {}
	const_ref(const T *data) : Super(data) {}
	const_ref(const ref<T> &ref) : Super(ref) {}
	const_ref(const This &ref) : Super(ref) {}

	This &operator=(const T *data) { assign(data); return *this; }
	This &operator=(const ref<T> &r) { assign(r); return *this; }
	This &operator=(const This &r) { assign(r); return *this; }

	operator const T*() const { return object(); }
	const T *operator->() const { return object(); }
	const T &operator*() const { return *object(); }

protected:
	T &object() const { return Super::object(); }
};

//---------------------------------------------------------------------------------------------

template <class T>
class ptr : public GeneralRef<T>
{
	friend class const_ref<T>;

	typedef GeneralRef<T> Super;
	typedef ptr<T> This;

public:
	typedef const_ptr<T> Const;

public:
	ptr() {}
	ptr(const Null &null) {}
	ptr(T *data) : Super(data) {}
	ptr(const ref<T> &r) : Super(r) {}
	ptr(const This &p) : Super(p) {}
	ptr(const GeneralRef<Object> &r) : Super(r) {}

	This &operator=(const Null &null) { Super::cleanup(); return *this; }
	This &operator=(T *data) { assign(data); return *this; }
	This &operator=(const ref<T> &r) { assign(r); return *this; }
	This &operator=(const This &p) { assign(p); return *this; }

	operator T&() { return object(); }
	operator const T&() const { return object(); }
	operator T*() { return &object(); }
	operator const T*() const { return &object(); }
	operator ref<Object>() const { return &object(); }
	operator ptr<Object>() const { return &object(); }
	
	bool operator!() const { return !refs(); }

	T *operator->() { return &object(); }
	const T *operator->() const { return &object(); }
	T &operator*() { return object(); }
	const T &operator*() const { return object(); }

protected:
	T &object() const { return Super::object(); }
};

//---------------------------------------------------------------------------------------------

template <class T>
class const_ptr : public GeneralRef<T>
{
	typedef GeneralRef<T> Super;
	typedef const_ptr<T> This;

public:
	const_ptr() {}
	const_ptr(const T *data) : Super(data) {}
	const_ptr(const ptr<T> &p) : Super(p) {}
	const_ptr(const This &p) : Super(p) {}

	This &operator=(const T *data) { assign(data); return *this; }
	This &operator=(const ptr<T> &p) { assign(p); return *this; }
	This &operator=(const This &r) { assign(r); return *this; }

	operator const T*() const { return object(); }
	const T *operator->() const { return object(); }
	const T &operator*() const { return *object(); }

protected:
	T &object() const { return Super::object(); }
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvfc

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif // _rvfc_memory_ref_
