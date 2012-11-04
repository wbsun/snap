#ifndef __HVP_RING_HH__
#define __HVP_RING_HH__

CLICK_DECLS

template <typename T>
class VarRing {
private:
	T *_ring;
	int _head, _tail, _next;
	int _capacity;

	void copy(const VarRing<T> &v);
public:
	explicit VarRing();
	explicit VarRing(int cap);
	VarRing(const VarRing<T> &v);
	~VarRing();
	VarRing<T> &operator=(const VarRing<T> &v);
	
	inline int size() const;
	inline bool empty() const;
	inline int capacity() const;
	bool reserve(int n);
	inline void clear();
	
	inline T& at(int i) {
		assert(!empty());
		int pos = _head + i;
		assert(pos <= _tail ||
		       (_tail < _head && pos <= _tail+_capacity));
		return _ring[pos%_capacity];
	}
	inline const T& at(int i) const {
		assert(!empty());
		int pos = _head + i;
		assert(pos <= _tail ||
		       (_tail < _head && pos <= _tail+_capacity));
		return _ring[pos%_capacity];
	}
	inline T& operator[](int i) {
		return at(i);
	}
	inline const T& operator[](int i) const {
		return at(i);
	}
	inline T& front() { return at(0); }
	inline const T& front() const { return at(0); }
	inline T& back() {
		assert(!empty());
		return _ring[_tail];
	}
	inline const T& back() const {
		assert(!empty());
		return _ring[_tail];
	}
	
	void check_and_makespace(int rsz);
	void push_front(T& v);
	void pop_front();
	void push_back(T& v);
	void pop_back();
};


/**
 * Single producer, single consumer.
 * Capacity never reached.
 * Producer just add_new().
 * Consumer just !empty(), oldest() and remove_oldest().
 */
template <typename T>
class LFRing {
private:
	T *_ring;
	int _head, _tail;
	int _capacity;

public:
	LFRing() : _ring(0), _head(0), _tail(0), _capacity(0) {}
	~LFRing() {
		if (_ring)
			delete[] _ring;
	}

	inline int size() const { return _head > _tail? _head-_tail:_capacity+1+_head-_tail; }
	inline int capacity() const { return _capacity; }
	bool reserve(int sz);
	inline void clear() { _head = _tail = 0; }
	bool empty() const { return _head == _tail; }

	inline T& at(int i) {
		assert(_head!=_tail);
		int pos = _tail + i;
		return _ring[pos%(_capacity+1)];
	}

	inline T& operator[](int i) {
		return at(i);
	}

	inline T& newest() {
		return _ring[(_head+capacity)%(_capacity+1)];
 		// return _ring[(_head-1+_capacity+1)%(_capacity+1)];
	}

	inline T& oldest() {
		return _ring[_tail];
	}

	bool add_new(T& v);
	void remove_oldest();
};

template <typename T> bool
LFRing<T>::reserve(int sz)
{
	assert(!_ring);
	assert(sz > 0);

	g4c_to_volatile(_ring) = (T*)CLICK_LALLOC(sizeof(T)*(sz+1));
	if (_ring) {
		g4c_to_volatile(_capacity) = sz;
		return true;
	} else
		return false;		
}

template <typename T> bool
LFRing<T>::add_new(T& v)
{
	assert(_ring);
	if ((_head+1)%(_capacity+1) == _tail)
		return false;
	_ring[_head] = v;
	asm volatile (
		""::"m"(_ring[_head]),"m"(_head)
		);
	g4c_to_volatile(_head) = (_head+1)%(_capacity+1);
	return true;	
}

template <typename T> void
LFRing<T>::remove_oldest()
{
	assert(_ring && _head!=_tail);
	g4c_to_volatile(_tail) = (_tail+1)%(_capacity+1);
}

CLICK_ENDDECLS

#include <click/ring.cc>

#endif
