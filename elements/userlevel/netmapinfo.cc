// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * netmapinfo.{cc,hh} -- library for interfacing with netmap
 * Eddie Kohler, Luigi Rizzo
 *
 * Copyright (c) 2012 Eddie Kohler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include "netmapinfo.hh"
#if HAVE_NET_NETMAP_H
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <click/sync.hh>
#include <unistd.h>
#include <fcntl.h>
CLICK_DECLS

static Spinlock netmap_memory_lock;
static void *netmap_memory = MAP_FAILED;
static size_t netmap_memory_size;
static uint32_t netmap_memory_users;

LFRing<unsigned char*> *NetmapInfo::buf_pools;
uint32_t *NetmapInfo::buf_consumer_locks;
int NetmapInfo::nr_threads;
bool NetmapInfo::initialized = false;

int
NetmapInfo::initialize(int nthreads, ErrorHandler *errh)
{
    if (nthreads < 1) {
	errh->fatal("NetmapInfo gets invalide #threads %d", nthreads);
	return -1;
    }

    if (!initialized) {
	nr_threads = nthreads;

	if (nr_threads == 1 || nr_buf_consumers = 1)
	    need_consumer_locking = false;
	else
	    need_consumer_locking = true;

	buf_pools = new LFRing<unsigned char*>[nr_threads];
	buf_consumer_locks = new uint32_t[nr_threads];
	if (buf_pools && buf_consumer_locks)
	{
	    for (int i=0; i<nr_threads; i++)
	    {
		if (!buf_pools[i].reserve(NM_BUF_SLOTS)) {
		    errh->fatal("NetmapInfo buf pool %d failt to reserve space",
				i);
		    return -1;
		}
		buf_consumer_locks[i] = 0;
	    }
	} else {
	    errh->fatal("Out of memory for NetmapInfo buf pools");
	    return -1;
	}
	
    }

    initialized = true;
    return 0;
}

int
NetmapInfo::ring::__open(const String &ifname, int ringid,
			 bool always_error, ErrorHandler *errh)
{
    ErrorHandler *initial_errh = always_error ? errh : ErrorHandler::silent_handler();

    int fd = ::open("/dev/netmap", O_RDWR);
    if (fd < 0) {
	if (ringid < 0)
	    initial_errh->error("/dev/netmap: %s", strerror(errno));
	else
	    initial_errh->error("/dev/netmap@%d: %s", ringid, strerror(errno));
	return -1;
    }

//     struct nmreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.nr_name, ifname.c_str(), sizeof(req.nr_name));
#if NETMAP_API
    req.nr_version = NETMAP_API;
#endif
    int r;
    if ((r = ioctl(fd, NIOCGINFO, &req))) {
	initial_errh->error("netmap %s: %s", ifname.c_str(), strerror(errno));
    error:
	close(fd);
	return -1;
    }

    if (ringid >= req.nr_rx_rings) {
	initial_errh->error("netmap: requested ringid %d larger/equal than "
			    "max ring number %u.", ringid, req.nr_rx_rings);
	goto error;
    }
    
    size_t memsize = req.nr_memsize;

    netmap_memory_lock.acquire();
    if (netmap_memory == MAP_FAILED) {
	netmap_memory_size = memsize;
	netmap_memory = mmap(0, netmap_memory_size, PROT_WRITE | PROT_READ,
			     MAP_SHARED, fd, 0);
	if (netmap_memory == MAP_FAILED) {
	    errh->error("netmap allocate %s: %s", ifname.c_str(), strerror(errno));
	    netmap_memory_lock.release();
	    goto error;
	}
    }
    mem = (char *) netmap_memory;
    ++netmap_memory_users;
    netmap_memory_lock.release();

    memset(&req, 0, sizeof(req));
    strncpy(req.nr_name, ifname.c_str(), sizeof(req.nr_name));
    req.nr_version = NETMAP_API;
    if (ringid < 0)
	req.nr_ringid = 0;
    else
	req.nr_ringid = ((uint16_t) ringid) | NETMAP_HW_RING;

    if ((r = ioctl(fd, NIOCREGIF, &req))) {
	errh->error("netmap register %s: %s", ifname.c_str(), strerror(errno));
	goto error;
    }


    nifp = NETMAP_IF(mem, req.nr_offset);
    return fd;
}

int
NetmapInfo::ring::open(const String &ifname,
		       bool always_error, ErrorHandler *errh)
{
    return __open(ifname, -1, always_error, errh);
}

int
NetmapInfo::ring::open_ring(const String &ifname, int ringid,
			    bool always_error, ErrorHandler *errh)
{
    return __open(ifname, ringid, always_error, errh);
}

void
NetmapInfo::ring::initialize_rings_rx(int timestamp)
{
    ring_begin = 0;
    // 0 means "same count as the converse direction"
    ring_end = nifp->ni_rx_rings ? nifp->ni_rx_rings : nifp->ni_tx_rings;
    if (timestamp >= 0) {
	int flags = (timestamp > 0 ? NR_TIMESTAMP : 0);
	for (unsigned i = ring_begin; i != ring_end; ++i)
	    NETMAP_RXRING(nifp, i)->flags = flags;
    }
}

void
NetmapInfo::ring::initialize_rings_tx()
{
    ring_begin = 0;
    ring_end = nifp->ni_tx_rings ? nifp->ni_tx_rings : nifp->ni_rx_rings;
}

void
NetmapInfo::ring::close(int fd)
{
    ioctl(fd, NIOCUNREGIF, &req);
    netmap_memory_lock.acquire();
    if (--netmap_memory_users <= 0 && netmap_memory != MAP_FAILED) {
	munmap(netmap_memory, netmap_memory_size);
	netmap_memory = MAP_FAILED;
    }
    netmap_memory_lock.release();
    ::close(fd);
}

CLICK_ENDDECLS
#endif
ELEMENT_PROVIDES(NetmapInfo)
