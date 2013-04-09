#ifndef CLICK_SNAP_CL_HH
#define CLICK_SNAP_CL_HH
#include <click/glue.hh>
#include <click/element.hh>
#include <click/pbatch.hh>
#include <g4c.h>
#include <g4c_cl.h>
using namespace std;

CLICK_DECLS

class SnapClassifier : public Element {
public:
    SnapClassifier();
    ~SnapClassifier();
    
    const char *class_name() const	{ return "SnapClassifier"; }
    int configure_phase() const	{ return CONFIGURE_PHASE_INFO; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

private:
    bool parse_patterns(Vector<String> &conf, ErrorHandler *errh, g4c_pattern_t *ptns, int n);
    void generate_random_patterns(g4c_pattern_t *ptns, int n);
    g4c_classifier_t *gcl;
    bool _debug;
};

CLICK_ENDDECLS
#endif
