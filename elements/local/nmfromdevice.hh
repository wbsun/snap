#ifndef CLICK_FROMDEVICE_USERLEVEL_HH
#define CLICK_FROMDEVICE_USERLEVEL_HH
#include <click/element.hh>
#include "elements/userlevel/kernelfilter.hh"

#define FROMDEVICE_ALLOW_LINUX 1
#define FROMDEVICE_ALLOW_NETMAP 1
#include "elements/userlevel/netmapinfo.hh"

# include <click/task.hh>

CLICK_DECLS

class NMFromDevice : public Element { public:	

    NMFromDevice();
    ~NMFromDevice();

    const char *class_name() const	{ return "NMFromDevice"; }
    const char *port_count() const	{ return "0/1-2"; }
    const char *processing() const	{ return PUSH; }

    enum { default_snaplen = 2046 };
    int configure_phase() const		{ return KernelFilter::CONFIGURE_PHASE_FROMDEVICE; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    inline String ifname() const	{ return _ifname; }
    inline int fd() const		{ return _fd; }	

    void selected(int fd, int mask);

    const NetmapInfo::ring *netmap() const { return _method == method_netmap ? &_netmap : 0; }
    inline int dev_ringid() { return _ringid; }

    bool run_task(Task *task);

    void kernel_drops(bool& known, int& max_drops) const;

  private:

    int _fd;
    Task _task;
    void emit_packet(WritablePacket *p, int extra_len, const Timestamp &ts);
    NetmapInfo::ring _netmap;
    int netmap_dispatch();

    bool _force_ip;
    int _burst;
    int _datalink;

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
    enum { method_default, method_netmap, method_pcap, method_linux };
    int _method;

    static String read_handler(Element*, void*);
    static int write_handler(const String&, Element*, void*, ErrorHandler*);
};

CLICK_ENDDECLS
#endif
