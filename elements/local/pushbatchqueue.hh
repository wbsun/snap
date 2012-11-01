#ifndef CLICK_PUSH_BATCH_QUEUE_HH
#define CLICK_PUSH_BATCH_QUEUE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/pbatch.hh>
#include <click/ring.hh>
#include <click/task.hh>
#include <g4c.h>
CLICK_DECLS

/**
 * Sync device stream.
 */
class PushBatchQueue : public Element {
public:
	PushBatchQueue();
	~PushBatchQueue();

	const char *class_name() const { return "PushBatchQueue"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }

	void push(int i, Packet *p); // Should never be called.
	void bpush(int i, PBatch *pb);

	bool run_task(Task *task);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	int initialize(ErrorHandler *errh);

	static const int DEFAULT_LEN;
private:
	int _que_len;
	LFRing<PBatch*> _que;
	Task _task;
	bool _block;
	bool _process_all;
	bool _fast_sched;
	int _drops;
}

CLICK_ENDDECLS
#endif
