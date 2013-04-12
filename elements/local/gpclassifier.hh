#ifndef CLICK_GCL_HH
#define CLICK_GCL_HH
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

class GPClassifier : public BElement {
public:
    GPClassifier();
    ~GPClassifier();
    
    const char *class_name() const	{ return "GPClassifier"; }
    int configure_phase() const	{ return CONFIGURE_PHASE_INFO; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

    int classify_batch(PBatch *pb);
    void bpush(int i, PBatch* pb);
    void push(int i, Packet* p);

private:
    int parse_rules(Vector<String> &conf, g4c_pattern_t *ptns, int n, ErrorHandler *errh);
    void generate_random_patterns(g4c_pattern_t *ptns, int n);
    Batcher* _batcher;
    g4c_classifier_t *gcl;
    int _test;
    int _on_cpu;
    bool _div;
    PSliceRange _psr;
    int16_t _anno_offset;
    int16_t _slice_offset;
};

CLICK_ENDDECLS
#endif
