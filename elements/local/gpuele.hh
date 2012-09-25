#ifndef CLICK_GPUELE_HH
#define CLICK_GPUELE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.hh>
#include <click/vector.hh>
#include <click/pair.hh>
#include <click/timer.hh>
#include <click/task.hh>
#include <click/deque.hh>
#include <g4c.h> // GPU for Click header
CLICK_DECLS

/**
 * This element doesn't consider thread-safe, though it uses
 * multi-threading itself.
 */
class GPUEle : public Element
{
public:
	typedef Deque<Packet*> PacketQueue;
	typedef Pair<PacketQueue*, unsigned char*> PacketBatch;

private:
	unsigned long _count;
	unsigned long _dropped;
	
	size_t _pktbufsz;
	unsigned int _batchsz;
	unsigned int _ustimeout;
	PacketBatch _curbatch;
	PacketBatch _pullbatch;
	Deque<PacketBatch> _todobatches;
	Deque<PacketBatch> _donebatches;

	Deque<PacketBatch> _batchpool;

	Task *_task;

	PacketBatch new_batch();
	bool init_batch_pool();
	void free_batch(PacketBatch pb);
	PacketBatch alloc_batch();
	void free_batch_pool();

public:
	static const size_t DEFAULT_PACKET_BUF_SZ = 4096; // Page size
	static const unsigned int DEFAULT_BATCH_SZ = 1024;
	static const unsigned int DEFAULT_BATCH_TIMEOUT = 1000; // 1ms
	static const unsigned int MAX_NR_BATCH = 1000;
public:

	GPUEle();
	~GPUEle();

	const char *class_name() const		{ return "GPUEle"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const          { return PUSH_TO_PULL; }

	int initialize(ErrorHandler *errh);
	int configure(Vector<String>&, ErrorHandler*);

	void run_timer(Timer *timer);
	bool run_task(Task *task);

	Packet *pull(int i);
	void push(int i, Packet *);

};

CLICK_ENDDECLS
#endif
