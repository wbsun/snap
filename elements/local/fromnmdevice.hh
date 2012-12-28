#ifndef CLICK_FROMNMDEVICE_HH
#define CLICK_FROMNMDEVICE_HH
#include <click/element.hh>
#include "elements/userlevel/kernelfilter.hh"
#include "elements/userlevel/netmapinfo.hh"

# include <click/task.hh>

CLICK_DECLS

class FromNMDevice : public Element { public:	

    FromNMDevice();
    ~FromNMDevice();

    const char *class_name() const	{ return "FromNMDevice"; }
    const char *port_count() const	{ return "0/1-2"; }
    const char *processing() const	{ return PUSH; }

    enum { default_snaplen = 2046 };
    int configure_phase() const 	{ return KernelFilter::CONFIGURE_PHASE_FROMDEVICE; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    inline String ifname() const
	{ return _ifname; }
    inline int fd() const
	{ return _fd; }	

    void selected(int fd, int mask);

    const NetmapInfo::ring *netmap() const
	{ return &_netmap; }
    inline int dev_ringid() { return _ringid; }

    bool run_task(Task *task);

  private:

    int _fd;
    Task _task;
    void emit_packet(WritablePacket *p, int extra_len,
		     const Timestamp &ts);
    NetmapInfo::ring _netmap;
    int netmap_dispatch();

    bool _test;

    bool _fd_added;

    bool _force_ip;
    int _burst;
    int _datalink;

    bool _full_nm;
    int _nm_fd;

#if HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    counter_t _count;

    String _ifname;
    int _ringid;
    bool _sniffer : 1;
    bool _promisc : 1;
    bool _outbound : 1;
    bool _timestamp : 1;
    int _was_promisc : 2;
    int _snaplen;
    unsigned _headroom;

    static String read_handler(Element*, void*);
    static int write_handler(const String&, Element*, void*,
			     ErrorHandler*);
};

CLICK_ENDDECLS
#endif
