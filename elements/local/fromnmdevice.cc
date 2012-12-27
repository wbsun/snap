// -*- mode: c++; c-basic-offset: 4 -*-

#include <click/config.h>
#include <sys/types.h>
#include <sys/time.h>
# include <sys/ioctl.h>
#include "fromnmdevice.hh"
#include <click/etheraddress.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <fcntl.h>
#include <click/master.hh>

# include <sys/socket.h>
# include <net/if.h>
# include <features.h>
# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
#  include <net/ethernet.h>
# else
#  include <net/if_packet.h>
#  include <linux/if_packet.h>
#  include <linux/if_ether.h>
# endif

# include <sys/mman.h>

CLICK_DECLS

FromNMDevice::FromNMDevice()
    :
      _task(this),
      _count(0), _promisc(0), _snaplen(0)
{
    _fd = -1;
    _ringid = -1;
    _test = false;
}

FromNMDevice::~FromNMDevice()
{
}

int
FromNMDevice::configure(Vector<String> &conf,
			ErrorHandler *errh)
{
    bool promisc = false, outbound = false,
	sniffer = true, timestamp = false;
    _snaplen = default_snaplen;
    _headroom = Packet::default_headroom;
    _headroom += (4 - (_headroom + 2) % 4) % 4; // default 4/2 alignment
    _force_ip = false;
    _burst = 1;
    _ringid = -1;
    String encap_type;
    bool has_encap;
    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _ifname)
	.read_p("PROMISC", promisc)
	.read_p("SNAPLEN", _snaplen)
	.read("SNIFFER", sniffer)
	.read("FORCE_IP", _force_ip)
	.read("OUTBOUND", outbound)
	.read("HEADROOM", _headroom)
	.read("ENCAP", WordArg(), encap_type).read_status(has_encap)
	.read("BURST", _burst)
	.read("TIMESTAMP", timestamp)
	.read("RING", _ringid)
	.read("TEST", _test)
	.complete() < 0)
	return -1;
    if (_snaplen > 8190 || _snaplen < 14)
	return errh->error("SNAPLEN out of range");
    if (_headroom > 8190)
	return errh->error("HEADROOM out of range");
    if (_burst <= 0)
	return errh->error("BURST out of range");

    _sniffer = sniffer;
    _promisc = promisc;
    _outbound = outbound;
    _timestamp = timestamp;

    NetmapInfo::register_buf_consumer();
    NetmapInfo::set_dev_dir(_ifname.c_str(),
			    NetmapInfo::dev_rx);
    
    return 0;
}

int
FromNMDevice::initialize(ErrorHandler *errh)
{
    if (!_ifname)
	return errh->error("interface not set");

    NetmapInfo::initialize(master()->nthreads(), errh);

    if (_ringid >= 0)
	_fd = _netmap.open_ring(_ifname, _ringid, true, errh);
    else
	_fd = _netmap.open(_ifname, true, errh);
    if (_fd >= 0) {
	_netmap.initialize_rings_rx(0);//_timestamp);
    }

    ScheduleInfo::initialize_task(this, &_task, false, errh);
    if (_fd >= 0)
	add_select(_fd, SELECT_READ);

    if (!_sniffer)
	if (KernelFilter::device_filter(_ifname, true, errh) < 0)
	    _sniffer = true;

    return 0;
}

#include <stdio.h>
void
FromNMDevice::cleanup(CleanupStage stage)
{
    if (stage >= CLEANUP_INITIALIZED && !_sniffer)
	KernelFilter::device_filter(
	    _ifname, false, ErrorHandler::default_handler());
    if (_fd >= 0)
	    _netmap.close(_fd);
    _fd = -1;
}

void
FromNMDevice::emit_packet(WritablePacket *p,
			  int extra_len,
			  const Timestamp &ts)
{
#if 0
    // set packet type annotation
    if (p->data()[0] & 1) {
	if (EtherAddress::is_broadcast(p->data()))
	    p->set_packet_type_anno(Packet::BROADCAST);
	else
	    p->set_packet_type_anno(Packet::MULTICAST);
    }

    // set annotations
    p->set_timestamp_anno(ts);
    p->set_mac_header(p->data());
    SET_EXTRA_LENGTH_ANNO(p, extra_len);

    if (!_force_ip || fake_pcap_force_ip(p, _datalink))
	output(0).push(p);
    else
	checked_output_push(1, p);
#endif
    output(0).push(p);
}

int
FromNMDevice::netmap_dispatch()
{
    int n = 0;
    for (unsigned ri = _netmap.ring_begin;
	 ri != _netmap.ring_end; ++ri) {
	struct netmap_ring *ring = NETMAP_RXRING(_netmap.nifp, ri);

	NetmapInfo::refill(ring);

	if (unlikely(_test))
	    click_chatter("netmap ring %u slots, av %u, rings %u",
			  ring->num_slots, ring->avail,
			  _netmap.ring_end - _netmap.ring_begin);

	if (ring->avail == 0)
	    continue;

	int nzcopy = (int) (ring->num_slots / 2) - (int) ring->reserved;

	while (n != _burst &&
	       ring->avail > 0) {
	    unsigned cur = ring->cur;
	    unsigned buf_idx = ring->slot[cur].buf_idx;
	    if (buf_idx < 2)
		break;
	    unsigned char *buf =
		(unsigned char *) NETMAP_BUF(ring, buf_idx);

	    WritablePacket *p;
	    if (nzcopy > 0) {
		p = Packet::make(buf,
				 ring->slot[cur].len,
				 NetmapInfo::buffer_destructor);
		++ring->reserved;
		--nzcopy;
	    } else {
		p = Packet::make(_headroom, buf, ring->slot[cur].len, 0);
		unsigned res1idx = NETMAP_RING_FIRST_RESERVED(ring);
		ring->slot[res1idx].buf_idx = buf_idx;
	    }

	    ring->cur = NETMAP_RING_NEXT(ring, ring->cur);
	    --ring->avail;
	    ++n;

	    emit_packet(p, 0, ring->ts);
	}
    }
    return n;
}

void
FromNMDevice::selected(int fd, int mask)
{
    if (! (mask & Element::SELECT_READ))
	return;
    int r = netmap_dispatch();
    if (r > 0) {
	_count += r;
	_task.reschedule();
    }
}

bool
FromNMDevice::run_task(Task *)
{
    // Read and push() at most one burst of packets.
    int r = 0;
    r = netmap_dispatch();
    if (r > 0) {
	_count += r;
	_task.fast_reschedule();
	return true;
    } else
	return false;
}

String
FromNMDevice::read_handler(Element* e, void *thunk)
{
    FromNMDevice* fd = static_cast<FromNMDevice*>(e);
    return String(fd->_count);
}

int
FromNMDevice::write_handler(
    const String &, Element *e, void *, ErrorHandler *)
{
    FromNMDevice* fd = static_cast<FromNMDevice*>(e);
    fd->_count = 0;
    return 0;
}

void
FromNMDevice::add_handlers()
{
    add_read_handler("count", read_handler, 0);
    add_write_handler("reset_counts",
		      write_handler,
		      0, Handler::BUTTON);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel KernelFilter NetmapInfo)
EXPORT_ELEMENT(FromNMDevice)
