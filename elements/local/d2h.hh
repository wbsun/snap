#ifndef CLICK_D2H_HH
#define CLICK_D2H_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/pbatch.hh>
#include <g4c.h>

CLICK_DECLS

class D2H : public Element {
public:
	D2H();
	~D2H();

	const char *class_name() const { return "D2H"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }

	void push(int i, Packet *p); // Should never be called.
	void bpush(int i, PBatch *pb);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	int initialize(ErrorHandler *errh);
};

CLICK_ENDDECLS
#endif
