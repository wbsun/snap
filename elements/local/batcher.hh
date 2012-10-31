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
 *   FORCE_PKTLENS: bool value.
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

	// Batcher/PBatch users are supposed to call the followings at their
	// configuration time.
	void set_slice_range(int begin, int end);
	void set_anno_flags(unsigned char flags);
	inline unsigned long set_batch_user_info(unsigned long priv_len)
		{
			unsigned long cur = _user_priv_len;
			
			_nr_users++;
			_user_priv_len += g4c_round_up(priv_len, 8); // for 64bit/8byte alignment.
			return cur;
		}

	static bool kill_batch(PBatch *pb);

private:
	int _batch_capacity;
	int _cur_batch_size;
	// should have a mutex or spin_lock to protect batch pointer.
	PBatch *_batch;
	int _slice_begin, _slice_end;
	unsigned char _anno_flags;
	bool _force_pktlens;

	int _nr_users;
	unsigned long _user_priv_len;

	int _timeout_ms;
	Timer _timer;
	PBatch *_timed_batch;

	int _count;
	int _drops;

	PBatch *alloc_batch();
	void add_packet(Packet *p);

};

CLICK_ENDDECLS
#endif
