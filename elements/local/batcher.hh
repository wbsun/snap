#ifndef CLICK_BATCHER_HH
#define CLICK_BATCHER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/velement.hh>
CLICK_DECLS

#define BATCHER_DEFAULT_SIZE 1024

class Batcher : public VElement {

	int _count;

public:

	Batcher();
	~Batcher();

	const char *class_name() const		{ return "Batcher"; }
	const char *port_count() const		{ return PORTS_1_1; }
	void *cast(const char *name);

	Packet *pull(int i);
	void push(int i, Packet *);
	void vpush(int port, Vector<Packet*> *ps);
	Vector<Packet*> *vpull(int port);
	

	const int batch_size() const { return BATCHER_DEFAULT_SIZE; };

};

CLICK_ENDDECLS
#endif
