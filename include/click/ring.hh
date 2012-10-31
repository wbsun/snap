#ifndef __HVP_RING_HH__
#define __HVP_RING_HH__

CLICK_DECLS

template <typename T>
class Ring {
private:
	T *_ring;
	int _head, _tail, _next;
	int _capacity;

	void copy(const Ring<T> &v);
public:
	explicit Ring();
	explicit Ring(int cap);
	Ring(const Ring<T> &v);
	~Ring();
	Ring<T> &operator=(const Ring<T> &v);
	
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

CLICK_ENDDECLS

#include <click/ring.cc>

#endif
