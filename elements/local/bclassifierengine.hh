#ifndef CLICK_BCL_HH
#define CLICK_BCL_HH
#include <click/glue.hh>
#include <click/element.hh>
#include <click/pbatch.hh>
#include "belement.hh"
#include "batcher.hh"
#include <g4c.h>
#include <g4c_cl.h>
#include <vector>
using namespace std;

CLICK_DECLS

class BClassifierEngine : public BElement {
public:
    BClassifierEngine();
    ~BClassifierEngine();
    
    const char *class_name() const	{ return "BClassifierEngine"; }
    const char *port_count() const	{ return "1-/1-"; }
    const char *processing() const  { return PUSH; }

    void push(int i, Packet *p);
    void bpush(int i, PBatch *p);
    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

private:
    void generate_random_patterns(g4c_pattern_t *ptns, int n);
    Batcher* _batcher;
    g4c_classifier_t *gcl;
    int _test;
    PSliceRange _psr;
    int16_t _anno_offset;
    int16_t _slice_offset;
    int _on_cpu;
    bool _div;
};

CLICK_ENDDECLS
#endif
