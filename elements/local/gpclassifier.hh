#ifndef CLICK_GCL_HH
#define CLICK_GCL_HH
#include <click/glue.hh>
#include <click/element.hh>
#include <click/pbatch.hh>
#include "belement.hh"
#include "batcher.hh"
#include "classifierruleset.hh"
#include <g4c.h>
#include <g4c_cl.h>
#include <vector>
using namespace std;

CLICK_DECLS

class GPClassifier : public BElement {
public:
    GPClassifier();
    ~GPClassifier();
    
    const char *class_name() const	{ return "GPClassifier"; }
    const char *port_count() const      { return "1-/1-"; }
    const char *processing() const      { return PUSH; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

    void bpush(int i, PBatch* pb);
    void push(int i, Packet* p);

private:
    Batcher* _batcher;
    ClassifierRuleset* _classifier;
    int _test;
    int _on_cpu;
    bool _div;
    PSliceRange _psr;
    int16_t _anno_offset;
    int16_t _slice_offset;
};

CLICK_ENDDECLS
#endif
