#ifndef CLICK_BIDS_HH
#define CLICK_BIDS_HH
#include <click/glue.hh>
#include <click/element.hh>
#include <click/pbatch.hh>
#include "belement.hh"
#include "batcher.hh"
#include <g4c.h>
#include <g4c_ac.h>
#include <vector>
using namespace std;

CLICK_DECLS

class BIDSMatcher : public BElement {
public:
    BIDSMatcher();
    ~BIDSMatcher();
    
    const char *class_name() const	{ return "BIDSMatcher"; }
    const char *port_count() const	{ return "1/1"; }
    const char *processing() const  { return PUSH; }

    void push(int i, Packet *p);
    void bpush(int i, PBatch *p);
    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

private:
    char ** generate_random_patterns(int np, int plen);
    Batcher* _batcher;
    g4c_acm_t *_acm;
    int _test;
    int _rand_pattern_max_len;
    PSliceRange _psr;
    int16_t _anno_offset;
    int16_t _slice_offset;
    int _on_cpu;
};

CLICK_ENDDECLS
#endif
