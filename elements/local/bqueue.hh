#ifndef CLICK_BQUEUE_HH
#define CLICK_BQUEUE_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/glue.hh>
#include <g4c.h>
#include <click/error.hh>
#include <click/ring.hh>
#include <click/pbatch.hh>
#include "belement.hh"

CLICK_DECLS

class BQueue : public BElement {
public:
    BQueue();
    ~BQueue();

    const char *class_name() const { return "BQueue"; }
    const char *port_count() const { return "-/1"; }
    const char *processing() const { return PUSH; }

    void push(int i, Packet *p); // Should never be called.
    void bpush(int i, PBatch *pb);

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

    static const int DEFAULT_LEN;

private:
    int _que_len;
    volatile uint32_t _q_prod_lock;
    volatile uint32_t _q_cons_lock;
    LFRing<PBatch*> _que;
    int _drops;
    bool _test;
    bool _onpush;
    int _checks;
};

CLICK_ENDDECLS
#endif
