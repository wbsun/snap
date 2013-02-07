#ifndef CLICK_H2D_HH
#define CLICK_H2D_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/pbatch.hh>
#include <g4c.h>
#include "belement.hh"

CLICK_DECLS

class H2D : public BElement {
public:
    H2D();
    ~H2D();

    const char *class_name() const { return "H2D"; }
    const char *port_count() const { return "1-/="; }
    const char *processing() const { return PUSH; }

    void push(int i, Packet *p); // Should never be called.
    void bpush(int i, PBatch *pb);

    void drop_batch(PBatch *pb);

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

private:
    bool _test;
};

CLICK_ENDDECLS
#endif
