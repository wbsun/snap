#ifndef __HVP_RING_CC__
#define __HVP_RING_CC__
#include <click/glue.hh>
#include <click/ring.hh>
#include <click/hvputils.hh>

CLICK_DECLS

template <typename T>
VarRing<T>::VarRing():_ring(0), _head(0), _tail(0), _capacity(0), _next(0)
{
}

template <typename T>
VarRing<T>::VarRing(int cap):_capacity(cap),
		       _ring(0),
		       _head(0), _tail(0), _next(0)
{
	_ring = (T*)CLICK_LALLOC(sizeof(T)*cap);
	if (!_ring)
		hvp_chatter("memory allocation failed\n");
}

template <typename T>
VarRing<T>::VarRing(const VarRing<T> &v)
{
	copy(v);	
}

template <typename T>
VarRing<T>::~VarRing()
{
	if (_ring && _capacity)
		CLICK_LFREE(_ring, _capacity*sizeof(T));
}

template <typename T>
void VarRing<T>::copy(const VarRing<T> &v)
{
	_capacity = v._capacity;
	_head = v._head;
	_tail = v._tail;
	_next = v._next;
	_ring = (T*)CLICK_LALLOC(sizeof(T)*_capacity);
	if (!_ring)
		hvp_chatter("memory allocation failed\n");
	else
		memcpy(_ring, v._ring, sizeof(T)*_capacity);	
}

template <typename T>
VarRing<T> &VarRing<T>::operator=(const VarRing<T> &v)
{
	clear();
	copy(v);
	return *this;
}

template <typename T>
inline int VarRing<T>::size() const
{
	if (_tail < _head)
		return _next+_capacity-_head;
	else
		retuen _tail - _head + 1;
}

template <typename T>
inline bool VarRing<T>::empty() const
{
	return _tail == _next || !_ring;
}

template <typename T>
inline int VarRing<T>::capacity() const
{
	return _capacity;
}

template <typename T>
bool VarRing<T>::reserve(int n);
{
	if (n <= size() || n == _capacity)
		return True;
	
	T* r =(T*)CLICK_LALLOC(sizeof(T)*n);
	if (r) {
		if (_ring && !empty()) // only !empty() needed
		{
			int offset = 0;
			if (_tail < _head) {
				memcpy(r, _ring+_head,
				       sizeof(T)*(_capacity-_head));
				memcpy(r+_capacity-_head, _ring,
				       sizeof(T)*(_tail+1));
				offset = _capacity-(_head-_tail);
			} else {
				memcpy(r, _ring+_head,
				       sizeof(T)*(_tail-_head+1));
				offset = _tail - _head;
			}
			
			_head = 0;
			_tail = offset;
			_next = (_tail+1)%n;
		} else {
			_head = _tail = _next = 0;
		}

		if (_ring)
			CLICK_LFREE(_ring, _capacity*sizeof(T));
		
		_capacity  = n;
		_ring = r;
	}
	else {
		hvp_chatter("memory allocation failed\n");
		return False;
	}

	return True;
}

template <typename T>
inline void VarRing<T>::clear()
{
	_head = _tail = _next = 0;
}

template <typename T>
void VarRing<T>::check_and_makespace(int rsz)
{
	if (!_ring || (_next == _head && _head != _tail)) {
		int sz;
		if (!_capacity)
			sz = rsz;
		else if (_ring)
			sz = _capacity<<1;
		else
			sz = _capacity;
			
		reserve(sz);
	}
}

template <typename T>
void VarRing<T>::push_front(T& v)
{
	check_and_makespace(10);
	if (empty()) 		
		_next = (_next+1)%_capacity;
	else
		_head = (_head-1)%_capacity;
		
	_ring[_head] = v;
}

template <typename T>
void VarRing<T>::pop_front()
{
	assert(!empty());
	assert(_ring);

	if (_head == _tail)
		_next = _head;
	else
		_head = (_head+1)%_capacity;
	
}

template <typename T>
void VarRing<T>::push_back(T& v)
{
	check_and_makespace(10);

	_next = (_next+1)%_capacity;
	if (!empty())
		_tail = (_tail+1)%_capacity;

	_ring[_tail] = v;
}

template <typename T>
void VarRing<T>::pop_back()
{
	assert(!empty());
	assert(_ring);

	if (_head != _tail)
		_tail = (_tail-1)%_capacity;
	_next = (_next-1)%_capacity;
}

CLICK_ENDDECLS

#endif
