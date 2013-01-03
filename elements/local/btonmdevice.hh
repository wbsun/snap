#ifndef CLICK_BTONMDEVICE_HH
#define CLICK_BTONMDEVICE_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include "elements/local/fromnmdevice.hh"
#include <click/pbatch.hh>
CLICK_DECLS


class BToNMDevice : public Element { public:

    BToNMDevice();
    ~BToNMDevice();

    const char *class_name() const	{ return "BToNMDevice"; }
    const char *port_count() const	{ return "1/0-2"; }
    const char *processing() const	{ return "l/h"; }
    const char *flags() const	{ return "S2"; }

    int configure_phase() const 	{ return KernelFilter::CONFIGURE_PHASE_TODEVICE; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    String ifname() const
	{ return _ifname; }
    int fd() const
	{ return _fd; }
    inline int dev_ringid()
	{ return _ringid; }
    bool run_task(Task *);
    void selected(int fd, int mask);
    int send_packets_nm();  

  protected:

    Task _task;
    Timer _timer;

    String _ifname;
    int _my_port;
    int _nr_ports;
    int _fd;
    NetmapInfo::ring _netmap;
    int netmap_send_packet(Packet *p);
    int _ringid;
    NotifierSignal _signal;

    PBatch *_q;
    int _cur;
    int _my_pkts;
    int _burst;

    int _full_nm;
    int _nm_fd;

    bool _debug;
    bool _my_fd;
    int _backoff;
    int _pulls;

    enum { h_debug, h_signal, h_pulls, h_q };
    FromNMDevice *find_fromnmdevice() const;
    int send_packet(Packet *p);
    static int write_param(
	const String &in_s, Element *e, void *vparam, ErrorHandler *errh);
    static String read_param(Element *e, void *thunk);

};

CLICK_ENDDECLS
#endif
