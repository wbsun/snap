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
#include <click/hvputils.hh>
#include <string.h>
#include <errno.h>
#include <poll.h>
CLICK_DECLS

static Spinlock netmap_memory_lock;
static void *netmap_memory = MAP_FAILED;
static size_t netmap_memory_size;
static uint32_t netmap_memory_users;

LFRing<unsigned char*> *NetmapInfo::buf_pools;
volatile uint32_t *NetmapInfo::buf_consumer_locks;
volatile uint32_t NetmapInfo::exception_buf_pool_lock;
int NetmapInfo::nr_threads = 0;
int NetmapInfo::nr_buf_consumers = 0;
bool NetmapInfo::initialized = false;
bool NetmapInfo::need_consumer_locking;
map<string, uint32_t> NetmapInfo::dev_dirs;

vector<NetmapInfo::nmpollfd*> NetmapInfo::poll_fds;

int NetmapInfo::nr_extra_bufs = 0;
ssize_t NetmapInfo::__buf_start = 0;
uint16_t NetmapInfo::__nr_buf_size = 2048;

void
NetmapInfo::alloc_extra_bufs(int fd)
{
    int nr_buf_pp = nr_extra_bufs/nr_threads;
    int idx[NETMAP_IOC_EXBUF_ARR_SZ];

    for (int i=0; i<nr_threads; i++) {
	for (int b=0; b<nr_buf_pp; b++) {
	    int r = ioctl(fd, NIOCALLOCBUF, idx);
	    if (r) {
		ErrorHandler::default_handler()->error(
		    "netmap alloc extra buf: %s",
		    strerror(errno));
		return;
	    }
	    for (int j=0; j<NETMAP_IOC_EXBUF_ARR_SZ; j++) {
		unsigned char *buf = (unsigned char*)
		    (__buf_start + idx[j]*__nr_buf_size);
		// assert(!buf_pools[i].full());
		buf_pools[i].add_new(buf);
	    }
	    if (b%10==0) // Give us some hope, ioctl is slow..
		click_chatter("pool %d, %d sets nm exbufs done\n",
			      i, b);
	}
    }
}

void
NetmapInfo::free_extra_bufs(int fd)
{
    //if (nr_extra_bufs <= 0)
    return;

    int idx[NETMAP_IOC_EXBUF_ARR_SZ];
    
    for (int i=0; i<nr_threads; i++) {
	while(!buf_pools[i].empty()) {
	    int j;
	    for (j=0; j<NETMAP_IOC_EXBUF_ARR_SZ
		     && !buf_pools[i].empty(); j++) {
		unsigned char *buf =
		    buf_pools[i].remove_and_get_oldest();
		idx[j] = (buf - ((unsigned char*)(__buf_start)))/__nr_buf_size;
	    }
	    for (; j<NETMAP_IOC_EXBUF_ARR_SZ; j++)
		idx[j] = -1;
	    
	    int r = ioctl(fd, NIOCFREEBUF, idx);
	    if (r) {
		ErrorHandler::default_handler()->error(
		    "netmap free extra buf: %s",
		    strerror(errno));
		return;
	    }
	}
    }
}

int
NetmapInfo::initialize(int nthreads, ErrorHandler *errh)
{
    if (nthreads < 1) {
	errh->fatal("NetmapInfo gets invalide #threads %d", nthreads);
	return -1;
    }

    if (!initialized) {
	poll_fds.clear();
	poll_fds.reserve(64);
	
	nr_threads = nthreads;

	if (nr_threads == 1 || nr_buf_consumers == 1)
	    need_consumer_locking = false;
	else
	    need_consumer_locking = true;

	buf_pools = new LFRing<unsigned char*>[nr_threads];
	buf_consumer_locks = new uint32_t[nr_threads];
	exception_buf_pool_lock = 0;
	if (buf_pools && buf_consumer_locks)
	{
	    for (int i=0; i<nr_threads; i++)
	    {
		if (!buf_pools[i].reserve(NM_BUF_SLOTS)) {
		    errh->fatal(
			"NetmapInfo buf pool %d failed to reserve space",
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
    if (!NetmapInfo::initialized) {
	errh->warning("NetmapInfo not initialized before calling ring::open!");
	NetmapInfo::initialize(2, errh);
    }
    
    ErrorHandler *initial_errh = always_error ?
	errh : ErrorHandler::silent_handler();

    int fd = ::open("/dev/netmap", O_RDWR);
    if (fd < 0) {
	if (ringid < 0)
	    initial_errh->error("/dev/netmap: %s", strerror(errno));
	else
	    initial_errh->error("/dev/netmap@%d: %s",
				ringid, strerror(errno));
	return -1;
    }

    memset(&req, 0, sizeof(req));
    strncpy(req.nr_name, ifname.c_str(), sizeof(req.nr_name));
#if NETMAP_API
    req.nr_version = NETMAP_API;
#endif
    int r;
    if ((r = ioctl(fd, NIOCGINFO, &req))) {
	initial_errh->error("netmap %s: %s",
			    ifname.c_str(), strerror(errno));
    error:
	close(fd);
	return -1;
    }

    if (ringid >= req.nr_rx_rings || ringid >= req.nr_tx_rings) {
	initial_errh->error(
	    "netmap: requested ringid %d larger/equal than "
	    "max ring number rx %u. tx %u",
	    ringid, req.nr_rx_rings, req.nr_tx_rings);
	goto error;
    }
    
    size_t memsize = req.nr_memsize;

    netmap_memory_lock.acquire();
    if (netmap_memory == MAP_FAILED) {
	netmap_memory_size = memsize;
	netmap_memory = mmap(0, netmap_memory_size, PROT_WRITE | PROT_READ,
			     MAP_SHARED, fd, 0);
	if (netmap_memory == MAP_FAILED) {
	    errh->error("netmap allocate %s: %s",
			ifname.c_str(), strerror(errno));
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

    map<string, uint32_t>::iterator ite =
	NetmapInfo::dev_dirs.find(string(ifname.c_str()));
    if (ite == NetmapInfo::dev_dirs.end() ||
	! (ite->second & NetmapInfo::dev_tx)) {
	req.nr_ringid |= NETMAP_NO_TX_POLL;
	dirs = NetmapInfo::dev_rx;
	errh->message("Netmap dev %s open with RX\n", ifname.c_str());
    } else {
	dirs = NetmapInfo::dev_rx | NetmapInfo::dev_tx;
	errh->message("Netmap dev %s open with RX and TX\n", ifname.c_str());
    }

    if ((r = ioctl(fd, NIOCREGIF, &req))) {
	errh->error("netmap register %s: %s",
		    ifname.c_str(), strerror(errno));
	goto error;
    }

    nifp = NETMAP_IF(mem, req.nr_offset);

    struct netmap_ring *sample_ring;

    if (ringid < 0) {
	per_ring = false;
	sample_ring = NETMAP_RXRING(nifp, 0);
    }
    else {
	per_ring = true;
	ring_begin = ringid;
	ring_end = ringid+1;
	sample_ring = NETMAP_RXRING(nifp, ring_begin);
    }

    netmap_memory_lock.acquire();
    if (NetmapInfo::__buf_start == 0) {
	NetmapInfo::__buf_start = (ssize_t)((char*)(sample_ring) + sample_ring->buf_ofs);
	NetmapInfo::__nr_buf_size = sample_ring->nr_buf_size;
	NetmapInfo::alloc_extra_bufs(fd);
    }
    netmap_memory_lock.release();
		      
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
    if (!per_ring) {
	ring_begin = 0;
	ring_end = nifp->ni_rx_rings ?
	    nifp->ni_rx_rings : nifp->ni_tx_rings;
    }
    
    if (timestamp >= 0) {
	int flags = (timestamp > 0 ? NR_TIMESTAMP : 0);
	for (unsigned i = ring_begin; i != ring_end; ++i)
	    NETMAP_RXRING(nifp, i)->flags = flags;
    }
}

void
NetmapInfo::ring::initialize_rings_tx()
{
    if (!per_ring) {
	ring_begin = 0;
	ring_end = nifp->ni_tx_rings ?
	    nifp->ni_tx_rings : nifp->ni_rx_rings;
    }
}

void
NetmapInfo::ring::close(int fd)
{
    ioctl(fd, NIOCUNREGIF, &req);
    netmap_memory_lock.acquire();
    if (--netmap_memory_users <= 0 && netmap_memory != MAP_FAILED) {
	munmap(netmap_memory, netmap_memory_size);
	netmap_memory = MAP_FAILED;
	NetmapInfo::free_extra_bufs(fd);	    
    }
    netmap_memory_lock.release();
    ::close(fd);
}

int
NetmapInfo::register_thread_poll(int fd, Element *e, uint32_t dir)
{
    for (int i=0; i<poll_fds.size(); i++) {
	nmpollfd* pfd = poll_fds[i];

	if (pfd->fd == fd) {
	    assert(i == pfd->idx);
	    
	    if (dir & dev_rx)
		pfd->rxe = e;
	    else
		pfd->txe = e;
	    
	    return pfd->idx;
	}
    }

    nmpollfd *pfd = new nmpollfd();
    pfd->fd = fd;
    pfd->tid = -1;
    pfd->timeout = 1000;
    if (dir & dev_rx) {
	pfd->rxe = e;
	pfd->txe = 0;
    }	    
    else {
	pfd->txe = e;
	pfd->rxe = 0;
    }
    pfd->running = 0;
    pfd->idx = poll_fds.size();
    poll_fds.push_back(pfd);

    return pfd->idx;    
}

int
NetmapInfo::run_fd_poll(int idx, int times)
{
    struct pollfd fds[1];
    nmpollfd *pfd = poll_fds[idx];

    fds[0].fd = pfd->fd;
    fds[0].events = (pfd->rxe?(POLLIN):0)|(pfd->txe?(POLLOUT):0);

    if (!atomic_uint32_t::compare_and_swap(pfd->running, 0, 1))
	return 1;

    int i;
    int timeout = pfd->timeout;
    while (pfd->running) {
	i = poll(fds, 1, timeout); 
	if (i < 0) {
	    hvp_chatter("Poll error %s\n", strerror(errno));
	    return -1;
	}

	if (i>0) {
	    if (fds[0].revents & POLLERR) {
		hvp_chatter("Poll return events %d", fds[0].revents);
		return -1;
	    }

	    if (pfd->rxe && (fds[0].revents & POLLIN))
		pfd->rxe->selected(pfd->fd, Element::SELECT_READ | FROM_NM);
	    if (pfd->txe && (fds[0].revents & POLLOUT))
		pfd->txe->selected(pfd->fd, Element::SELECT_WRITE | FROM_NM);
	}

	if (times > 0) {
	    --times;
	    if (times <= 0) {
		atomic_uint32_t::swap(pfd->running, 0);
		return 2;
	    }
	}
    }

    return 0;
}

CLICK_ENDDECLS
#endif
ELEMENT_PROVIDES(NetmapInfo)
