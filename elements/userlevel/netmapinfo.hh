#ifndef CLICK_NETMAPINFO_HH
#define CLICK_NETMAPINFO_HH 1
#if HAVE_NET_NETMAP_H
#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_user.h>
#include <click/packet.hh>
#include <click/error.hh>
#include <click/sync.hh>
#include <click/ring.hh>
#include <click/glue.hh>
CLICK_DECLS

#ifndef NM_BUF_SLOTS
#define NM_BUF_SLOTS 65536
#endif

class NetmapInfo {
public:
    struct ring {
	char *mem;
	unsigned ring_begin;
	unsigned ring_end;
	struct netmap_if *nifp;
	struct nmreq req;
	bool rx, tx;
	bool per_ring;

	ring() {
	    rx = false;
	    tx = false;
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

    static unsigned char *buffers;	// XXX not thread safe

    static LFRing<unsigned char*> *buf_pools;
    static uint32_t *buf_consumer_locks;
    static int nr_buf_consumers;
    static int nr_threads;
    static bool initialized;
    static bool need_consumer_locking;

    static void register_buf_consumer() {
	nr_buf_consumers++;
    }	

    static int initialize(int nthreads, ErrorHandler *errh);
    
    static bool is_netmap_buffer(Packet *p) {
	return p->buffer_destructor() == buffer_destructor;
    }
    static void buffer_destructor(unsigned char *buf, size_t) {
	*reinterpret_cast<unsigned char **>(buf) = buffers;
	buffers = buf;
    }
    static bool refill(struct netmap_ring *ring) {
	bool rt = false;
	if (buffers) {
	    unsigned char *buf = buffers;
	    buffers = *reinterpret_cast<unsigned char **>(buffers);
	    unsigned res1idx = NETMAP_RING_FIRST_RESERVED(ring);
	    ring->slot[res1idx].buf_idx = NETMAP_BUF_IDX(ring, (char *) buf);
	    ring->slot[res1idx].flags |= NS_BUF_CHANGED;
	    --ring->reserved;
	    rt = true;
	}
	return rt;
    }

};

CLICK_ENDDECLS
#endif
#endif
