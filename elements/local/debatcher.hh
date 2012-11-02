#ifndef CLICK_DEBATCHER_HH
#define CLICK_DEBATCHER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/pbatch.hh>
#include <g4c.h>
CLICK_DECLS

class DeBatcher : public Element {
public:
	DeBatcher();
	~DeBatcher();

	const char *class_name() const { return "DeBatcher"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	void push(int i, Packet *p); // should never be called.
	void bpush(int i, PBatch *pb);

	Packet *pull(int port); 
	PBatch *bpull(int port); // should never be called.

	int configure(Vector<String> &conf, ErrorHandler *errh);
	int initialize(ErrorHandler *errh);

private:
	PBatch *_batch;
	int _idx;
};

CLICK_ENDDECLS
#endif
