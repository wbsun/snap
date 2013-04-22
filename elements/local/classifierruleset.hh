#ifndef CLICK_SNAP_CLRS_HH
#define CLICK_SNAP_CLRS_HH
#include <click/glue.hh>
#include <click/element.hh>
#include <click/pbatch.hh>
#include <g4c.h>
#include <g4c_cl.h>
using namespace std;

CLICK_DECLS

class ClassifierRuleset : public Element {
public:
    ClassifierRuleset();
    ~ClassifierRuleset();
    
    const char *class_name() const	{ return "ClassifierRuleset"; }
    // earlier than classifiers, but later than GPURuntime
    int configure_phase() const	{ return CONFIGURE_PHASE_PRIVILEGED; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

    enum {
	CLASSIFY_DEFAULT=0,
	CLASSIFY_ON_GPU=0,
	CLASSIFY_ON_CPU=1,
	CLASSIFY_CPU_NON_BATCH=2,
	CLASSIFY_THROUGH=3 };
    int classify_packets(PBatch *pb,
			 int16_t anno_ofs,
			 int16_t slice_ofs,
			 int process_option = CLASSIFY_DEFAULT);
    int classify_packet(Packet *p,
			int16_t anno_ofs,
			int16_t slice_ofs,
			int process_option);

private:
    bool parse_rules(Vector<String> &conf, ErrorHandler *errh, g4c_pattern_t *ptns, int n);
    void generate_random_rules(g4c_pattern_t *ptns, int n);
    g4c_classifier_t *gcl;
    bool _debug;
    int _nr_dsts;
};

CLICK_ENDDECLS
#endif
