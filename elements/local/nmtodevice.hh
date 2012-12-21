#ifndef CLICK_TODEVICE_USERLEVEL_HH
#define CLICK_TODEVICE_USERLEVEL_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include "elements/local/nmfromdevice.hh"
CLICK_DECLS

#define TODEVICE_ALLOW_LINUX 1
#define TODEVICE_ALLOW_NETMAP 1


class NMToDevice : public Element { public:

    NMToDevice();
    ~NMToDevice();

    const char *class_name() const		{ return "NMToDevice"; }
    const char *port_count() const		{ return "1/0-2"; }
    const char *processing() const		{ return "l/h"; }
    const char *flags() const			{ return "S2"; }

    int configure_phase() const { return KernelFilter::CONFIGURE_PHASE_TODEVICE; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    String ifname() const			{ return _ifname; }
    int fd() const				{ return _fd; }
    inline int dev_ringid() { return _ringid; }
    bool run_task(Task *);
    void selected(int fd, int mask);

  protected:

    Task _task;
    Timer _timer;

    String _ifname;
    int _fd;
    NetmapInfo::ring _netmap;
    int netmap_send_packet(Packet *p);
    int _ringid;
    NotifierSignal _signal;

    Packet *_q;
    int _burst;

    bool _debug;
    bool _my_fd;
    int _backoff;
    int _pulls;

    enum { h_debug, h_signal, h_pulls, h_q };
    NMFromDevice *find_nmfromdevice() const;
    int send_packet(Packet *p);
    static int write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh);
    static String read_param(Element *e, void *thunk);

};

CLICK_ENDDECLS
#endif
