#ifndef CLICK_NETMAPINFO_HH
#define CLICK_NETMAPINFO_HH 1
#if HAVE_NET_NETMAP_H
#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_user.h>
#include <click/packet.hh>
#include <click/error.hh>
#include <click/sync.hh>
CLICK_DECLS

class NetmapInfo {
public:
    struct ring {
	char *mem;
	unsigned ring_begin;
	unsigned ring_end;
	struct netmap_if *nifp;
	struct nmreq req;

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
    static Spinlock buffers_lock;
    
    static bool is_netmap_buffer(Packet *p) {
	return p->buffer_destructor() == buffer_destructor;
    }
    static void buffer_destructor(unsigned char *buf, size_t) {
//	buffers_lock.acquire();
	*reinterpret_cast<unsigned char **>(buf) = buffers;
	buffers = buf;
//	buffers_lock.release();
    }
    static bool refill(struct netmap_ring *ring) {
	bool rt = false;
//	buffers_lock.acquire();
	if (buffers) {
	    unsigned char *buf = buffers;
	    buffers = *reinterpret_cast<unsigned char **>(buffers);
	    unsigned res1idx = NETMAP_RING_FIRST_RESERVED(ring);
	    ring->slot[res1idx].buf_idx = NETMAP_BUF_IDX(ring, (char *) buf);
	    ring->slot[res1idx].flags |= NS_BUF_CHANGED;
	    --ring->reserved;
	    rt = true;
	}
//	buffers_lock.release();
	return rt;
    }

};

CLICK_ENDDECLS
#endif
#endif
