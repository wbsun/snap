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


template <class T>
class LFRing {
public:
    T* data;
    int pad1[0];
    
    int head;
    int pad2[0];
    
    int tail;
    int pad3[0];
    
    unsigned int cpo; // capacity plus one
    unsigned int mask;

    LFRing() : data(0), head(0),
		   tail(0), cpo(0), mask(0)
	{}

    LFRing(unsigned int cap_plus_one) {
	reserve(cap_plus_one);
    }

    ~LFRing() {
	if (data)
	    delete[] data;
    }

    bool reserve(unsigned int cap_plus_one) {
	if (__builtin_popcount(cap_plus_one>>1) != 1) {
	    ErrorHandler::default_handler()->fatal(
		"Capacity plus one %u not power of 2.",
		cap_plus_one);
	    return false;
	}

	cpo = cap_plus_one;
	mask = cpo-1;

	head = tail = 0;
	if (data = new T[cpo])
	    return true;
	else
	    return false;
    }

    inline unsigned int size() {
	return (head-tail)&mask;
    }
    
    inline unsigned int capacity() { return mask; }

    inline void clear() { head = tail = 0; }

    bool empty() { return head == tail; }

    bool full() { return ((head-tail)&mask) == mask; }

    T& oldest() { return data[tail]; }

    void add_new(T& v) {
	data[head] = v;
	__asm__ volatile("": :"m" (data[head]), "m" (head));
	head = (head+1)&mask;
	__asm__ volatile(""::"m"(head));
    }

    void remove_oldest() {
	tail = (tail+1)&mask;
	__asm__ volatile(""::"m"(tail));	    
    }

    void remove_oldest_with_wmb() {
	__asm__ volatile("": :"m" (data[tail]), "m" (tail));
	remove_oldest();
    }

    T& remove_and_get_oldest() {
	T& v = data[tail];
	__asm__ volatile("": :"m" (data[tail]), "m" (tail));
	tail = (tail+1)&mask;
	__asm__ volatile(""::"m" (tail));
	return v;
    }
};

CLICK_ENDDECLS

#include <click/ring.cc>

#endif
