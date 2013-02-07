#ifndef CLICK_BATCH_DISCARD_HH
#define CLICK_BATCH_DISCARD_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/pbatch.hh>
#include "belement.hh"
CLICK_DECLS

class BatchDiscard : public BElement { public:

    BatchDiscard();

    const char *class_name() const { return "BatchDiscard"; }
    const char *port_count() const { return PORTS_1_0; }
    const char *proceesing() const { return AGNOSTIC; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

    void bpush(int, PBatch *);
    void push(int, Packet*);
    bool run_task(Task *);

  protected:

    Task _task;

#if HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    counter_t _count;

    unsigned _burst;
    bool _active;

    enum { h_reset_counts = 0, h_active = 1 };
};

CLICK_ENDDECLS
#endif
