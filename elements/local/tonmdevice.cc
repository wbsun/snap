#include <click/config.h>
#include "tonmdevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <stdio.h>
#include <unistd.h>
#include <click/master.hh>

# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <net/if_packet.h>
# include <features.h>
# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
# else
#  include <linux/if_packet.h>
# endif

#include <sys/mman.h>


CLICK_DECLS

ToNMDevice::ToNMDevice()
    : _task(this), _timer(&_task), _q(0), _pulls(0)
{
    _fd = -1;
    _my_fd = false;
    _ringid = -1;
    _full_nm = true;
    _nm_fd = -1;
}

ToNMDevice::~ToNMDevice()
{
}

int
ToNMDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 1;
    _ringid = -1;
    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _ifname)
	.read("DEBUG", _debug)
	.read("BURST", _burst)
	.read("RING", _ringid)
	.read("FULL_NM", _full_nm)
	.complete() < 0)
	return -1;
    if (!_ifname)
	return errh->error("interface not set");
    if (_burst <= 0)
	return errh->error("bad BURST");

    NetmapInfo::set_dev_dir(_ifname.c_str(), NetmapInfo::dev_tx);
    return 0;
}

FromNMDevice *
ToNMDevice::find_fromnmdevice() const
{
    Router *r = router();
    for (int ei = 0; ei < r->nelements(); ++ei) {
	FromNMDevice *fd =
	    (FromNMDevice *) r->element(ei)->cast("FromNMDevice");
	if (fd && fd->ifname() == _ifname
	    && fd->dev_ringid() == _ringid
	    && fd->fd() >= 0)
	    return fd;
    }
    return 0;
}

int
ToNMDevice::initialize(ErrorHandler *errh)
{
    _timer.initialize(this);

    FromNMDevice *fd = find_fromnmdevice();
    if (fd && fd->netmap()) {
	_fd = fd->fd();
	_netmap = *fd->netmap();
    } else {
	NetmapInfo::initialize(master()->nthreads(), errh);
	if (_ringid >= 0)
	    _fd = _netmap.open_ring(_ifname, _ringid,
				    true, errh);
	else
	    _fd = _netmap.open(_ifname, true, errh);
	if (_fd >= 0) {
	    _my_fd = true;
	    if (!_full_nm)
		add_select(_fd, SELECT_READ); // NB NOT writable!
	} else
	    return -1;
    }
    if (_fd >= 0) {
	_netmap.initialize_rings_tx();

	if (_full_nm)
	    _nm_fd = NetmapInfo::register_thread_poll(_fd, this, NetmapInfo::dev_tx);
    }

    // check for duplicate writers
    void *&used = router()->force_attachment("device_writer_" + _ifname);
    if (used)
	return errh->error("duplicate writer for device %<%s%>", _ifname.c_str());
    used = this;

    ScheduleInfo::join_scheduler(this, &_task, errh);
//    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    return 0;
}

void
ToNMDevice::cleanup(CleanupStage)
{
    if (_full_nm && _nm_fd >= 0) {
	NetmapInfo::poll_fds[_nm_fd]->running = 0;
    }

    if (_fd >= 0 && _my_fd) {
	_netmap.close(_fd);
	_fd = -1;
    }
}


int
ToNMDevice::netmap_send_packet(Packet *p)
{
    for (unsigned ri = _netmap.ring_begin; ri != _netmap.ring_end; ++ri) {
	struct netmap_ring *ring = NETMAP_TXRING(_netmap.nifp, ri);
	if (ring->avail == 0)
	    continue;
	unsigned cur = ring->cur;
	unsigned buf_idx = ring->slot[cur].buf_idx;
	if (buf_idx < 2)
	    continue;
	unsigned char *buf = (unsigned char *) NETMAP_BUF(ring, buf_idx);
	uint32_t p_length = p->length();
	if (NetmapInfo::is_netmap_buffer(p)
	    && !p->shared() /* A little risk: && p->buffer() == p->data() */
	    && noutputs() == 0) {
	    ring->slot[cur].buf_idx = NETMAP_BUF_IDX(ring, (char *) p->buffer());
	    ring->slot[cur].flags |= NS_BUF_CHANGED;
	    NetmapInfo::buffer_destructor(buf, 0);
	    p->reset_buffer();
	} else
	    memcpy(buf, p->data(), p_length);
	ring->slot[cur].len = p_length;

	// need this?
//	__asm__ volatile("" : : : "memory");
	ring->cur = NETMAP_RING_NEXT(ring, cur);
	ring->avail--;
	return 0;
    }
    if (!_full_nm)
	errno = ENOBUFS;
    return -1;
}

/*
 * Linux select marks datagram fd's as writeable when the socket
 * buffer has enough space to do a send (sock_writeable() in
 * sock.h). BSD select always marks datagram fd's as writeable
 * (bpf_poll() in sys/net/bpf.c) This function should behave
 * appropriately under both.  It makes use of select if it correctly
 * tells us when buffers are available, and it schedules a backoff
 * timer if buffers are not available.
 * --jbicket
 */
int
ToNMDevice::send_packet(Packet *p)
{
    int r = 0;
    errno = 0;

    r = netmap_send_packet(p);

    if (r >= 0)
	return 0;
    else
	return errno ? -errno : -EINVAL;
}

int
ToNMDevice::send_packets_nm()
{
    Packet *p = _q;
    _q = 0;
    int count = 0, r=0;

    do {
	if (!p) {
	    if (!(p = input(0).pull()))
		break;
	}
	
	if ((r = netmap_send_packet(p)) >= 0) {
	    p->kill();
	    p=0;
	} else {
	    _backoff = 1;
	    _q = p;
	    break;
	}
    } while (count < _burst);
    
    return 0;
}

bool
ToNMDevice::run_task(Task *)
{
    int r = 0;
    if (_full_nm) {
	r = NetmapInfo::run_fd_poll(_nm_fd);
	if (!r)
	    return false;
	else
	    return true;
    }
    
    Packet *p = _q;
    _q = 0;
    int count = 0;

    do {
	if (!p) {
	    ++_pulls;
	    if (!(p = input(0).pull()))
		break;
	}
	if ((r = send_packet(p)) >= 0) {
	    _backoff = 0;
	    checked_output_push(0, p);
	    ++count;
	    p = 0;
	} else
	    break;
    } while (count < _burst);

    if (r == -ENOBUFS || r == -EAGAIN) {
	assert(!_q);
	_q = p;

	if (!_full_nm) {
	    if (!_backoff) {
		_backoff = 1;
		add_select(_fd, SELECT_WRITE);
	    } else {
		_timer.schedule_after(Timestamp::make_usec(_backoff));
		if (_backoff < 256)
		    _backoff *= 2;
		if (_debug) {
		    Timestamp now = Timestamp::now();
		    click_chatter(
			"%p{element} backing off for %d at %p{timestamp}\n", this, _backoff, &now);
		}
	    }		
	}
	
	return count > 0;
    } else if (r < 0) {
	click_chatter("ToNMDevice(%s): %s", _ifname.c_str(), strerror(-r));
	checked_output_push(1, p);
    }

    if (p && !_full_nm)
	_task.fast_reschedule();
    return count > 0;
}

void
ToNMDevice::selected(int, int)
{
    if (_full_nm) {
	send_packets_nm();
    } else {
	_task.reschedule();
	remove_select(_fd, SELECT_WRITE);
    }
}


String
ToNMDevice::read_param(Element *e, void *thunk)
{
    ToNMDevice *td = (ToNMDevice *)e;
    switch((uintptr_t) thunk) {
    case h_debug:
	return String(td->_debug);
    case h_signal:
	return String(td->_signal);
    case h_pulls:
	return String(td->_pulls);
    case h_q:
	return String((bool) td->_q);
    default:
	return String();
    }
}

int
ToNMDevice::write_param(const String &in_s, Element *e, void *vparam,
		     ErrorHandler *errh)
{
    ToNMDevice *td = (ToNMDevice *)e;
    String s = cp_uncomment(in_s);
    switch ((intptr_t)vparam) {
    case h_debug: {
	bool debug;
	if (!BoolArg().parse(s, debug))
	    return errh->error("type mismatch");
	td->_debug = debug;
	break;
    }
    }
    return 0;
}

void
ToNMDevice::add_handlers()
{
    add_task_handlers(&_task);
    add_read_handler("debug", read_param, h_debug, Handler::CHECKBOX);
    add_read_handler("pulls", read_param, h_pulls);
    add_read_handler("signal", read_param, h_signal);
    add_read_handler("q", read_param, h_q);
    add_write_handler("debug", write_param, h_debug);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FromNMDevice userlevel)
EXPORT_ELEMENT(ToNMDevice)
