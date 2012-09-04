#ifndef CLICK_BATCHER_HH
#define CLICK_BATCHER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
CLICK_DECLS

class Batcher : public VElement {

	int _count;

public:

	Batcher();
	~Batcher();

	const char *class_name() const		{ return "Batcher"; }
	const char *port_count() const		{ return PORTS_1_1; }

	Packet *pull(int i);
	void push(int i, Packet *);

};

CLICK_ENDDECLS
#endif
