#ifndef CLICK_BATCHER_HH
#define CLICK_BATCHER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/pbatch.hh>
#include <g4c.h>
CLICK_DECLS

/**
 * Batcher configurations:
 *   TIMEOUT: int value in mili-sec.
 *   SLICE_BEGIN: int value
 *   SLICE_END: int value
 *   CAPACITY: int value for batch capacity
 *   ANN_FLAGS: unsigned char.
 *   FORCE_PKTLENS: int value.
 *   
 */
class Batcher : public Element {
public:
	Batcher();
	~Batcher();

	const char *class_name() const	{ return "Batcher"; }
	const char *port_count() const	{ return PORTS_1_1; }
	const char *processing() const  { return PUSH; }

	void push(int i, Packet *p);
	int configure(Vector<String> &conf, ErrorHandler *errh);
	int initialize(ErrorHandler *errh);

	void run_timer(Timer *timer);

	void set_slice_range(int begin, int end);
	void set_anno_flags(unsigned char flags);

private:
	int _batch_capacity;
	int _cur_batch_size;
	// should have a mutex or spin_lock to protect batch pointer.
	PBatch *_batch;
	int _slice_begin, _slice_end;
	unsigned char _anno_flags;
	bool _force_pktlens;

	int _timeout_ms;
	Timer _timer;	

	int _count;
	int _drops;

	PBatch *alloc_batch();
	void add_packet(Packet *p);

};

CLICK_ENDDECLS
#endif
