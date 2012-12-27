#ifndef CLICK_NETMAPINFO_HH
#define CLICK_NETMAPINFO_HH 1
#if HAVE_NET_NETMAP_H
#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_user.h>
#include <click/packet.hh>
#include <click/error.hh>
#include <click/sync.hh>
#include <g4c.h>
#include <click/ring.hh>
#include <click/glue.hh>
#include <map>
#include <string>
using namespace std;

CLICK_DECLS

#ifndef NM_BUF_SLOTS
#define NM_BUF_SLOTS (1<<17)
#endif

class NetmapInfo {
public:
    struct ring {
	char *mem;
	unsigned ring_begin;
	unsigned ring_end;
	struct netmap_if *nifp;
	struct nmreq req;
	uint32_t dirs;
	bool per_ring;

	ring() {
	    dirs = 0;
	    per_ring = false;
	}

	int open(const String &ifname,
		 bool always_error, ErrorHandler *errh);
	int open_ring(const String &ifname, int ringid,
		      bool always_error, ErrorHandler *errh);
	void initialize_rings_rx(int timestamp);	    
	void initialize_rings_tx();
	void close(int fd);

    private:
	int __open(const String &ifname, int ringid,
		   bool always_error, ErrorHandler *errh);
    };

    static LFRing<unsigned char*> *buf_pools;
    static volatile uint32_t *buf_consumer_locks;
    static volatile uint32_t exception_buf_pool_lock;
    static int nr_buf_consumers;
    static int nr_threads;
    static bool initialized;
    static bool need_consumer_locking;

    enum { dev_rx = 0x1, dev_tx = 0x2 };

    static map<string, uint32_t> dev_dirs;

    static void set_dev_dir(const char *dev, uint32_t dirs) {
	map<string, uint32_t >::iterator ite = dev_dirs.find(string(dev));
	if (ite == dev_dirs.end()) {
	    dev_dirs[string(dev)] = 0;
	    ite = dev_dirs.find(string(dev));
	}

	ite->second |= dirs;
    }

    static void register_buf_consumer() {
	nr_buf_consumers++;
    }	

    static int initialize(int nthreads, ErrorHandler *errh);
    
    static bool is_netmap_buffer(Packet *p) {
	return p->buffer_destructor() == buffer_destructor;
    }
    static void buffer_destructor(unsigned char *buf, size_t) {
//	int tid = click_current_thread_id;
	LFRing<unsigned char*>* pool = buf_pools+click_current_thread_id;

#if 0
	if (unlikely(tid >= nr_threads))
	{
	    click_chatter("Bad thread id %d catched at buffer dtor\n", tid);
	    pool = buf_pools + nr_threads;
	    if (nr_threads > 1)
		while (atomic_uint32_t::swap(exception_buf_pool_lock, 1)
		       == 1);
	}
#endif
	
	if (!pool->full())
	{
	    pool->add_new(buf);	    
	} else
	    click_chatter("NetmapInfo buffer pool full! Can't handle this, should abort!");


#if 0
	if (unlikely(tid >= nr_threads) && nr_threads > 1)
	{
	    click_compiler_fence();
	    exception_buf_pool_lock = 0;
	}
#endif
    }
    
    static bool refill(struct netmap_ring *ring, bool multiple=true) {
	bool rt = false;
	int i, tid = click_current_thread_id;

	// Start from local thread to reduce racing.
	for (int j=0; j<nr_threads && ring->reserved > 0; ++j)
	{
	    i =  (tid+j)%nr_threads;
	    
	    if (need_consumer_locking)
	    {
		while(atomic_uint32_t::swap(buf_consumer_locks[i], 1) == 1);
	    }
	    
	    while (!buf_pools[i].empty() && ring->reserved > 0)
	    {
		unsigned char *buf = buf_pools[i].remove_and_get_oldest();
		unsigned res1idx = NETMAP_RING_FIRST_RESERVED(ring);
		
		ring->slot[res1idx].buf_idx = NETMAP_BUF_IDX(ring, (char *) buf);
		ring->slot[res1idx].flags |= NS_BUF_CHANGED;
		--ring->reserved;
		if (unlikely(!rt))
		    rt = true;

		// ignore single case
//		else if (unlikely(!multiple))
//		    goto unlock_and_out;
	    }
	    
	    if (need_consumer_locking)
	    {
		click_compiler_fence();
		buf_consumer_locks[i] = 0;
	    }
	}
	return rt;
    }

};

CLICK_ENDDECLS
#endif
#endif
