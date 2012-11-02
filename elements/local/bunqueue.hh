#ifndef CLICK_BUNQUEUE_HH
#define CLICK_BUNQUEUE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/pbatch.hh>
#include <g4c.h>
#include <click/ring.hh>
CLICK_DECLS

class BUnqueue : public Element {
public:
	BUnqueue();
	~BUnqueue();

	const char *class_name() const { return "BUnqueue"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH_TO_PULL; }

	void push(int i, Packet *p); // Should never be called.
	void bpush(int i, PBatch *pb);

	Packet *pull(int port); // Should never be called.
	PBatch *bpull(int port);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	int initialize(ErrorHandler *errh);

	static const int DEFAULT_LEN;

private:
	int _que_len;
	LFRing<PBatch*> _que;
	int _drops;
};

CLICK_ENDDECLS
#endif
