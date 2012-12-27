#include <click/config.h>
#include "pushbatchqueue.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/timestamp.hh>
CLICK_DECLS

const int PushBatchQueue::DEFAULT_LEN = (int)(1<<16);

PushBatchQueue::PushBatchQueue() : _task(this), _que_len(DEFAULT_LEN),
				   _block(false), _process_all(false),
				   _fast_sched(false), _test(false),
				   _sched_on_new(false)
{
}

PushBatchQueue::~PushBatchQueue()
{
    // kill all remaining batches?
}

int
PushBatchQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
		     "LENGTH", cpkN, cpInteger, &_que_len,
		     "BLOCK", cpkN, cpBool, &_block,
		     "PROCESS_ALL", cpkN, cpBool, &_process_all,
		     "FAST_SCHED", cpkN, cpBool, &_fast_sched,
		     "TEST", cpkN, cpBool, &_test,
		     "SCHED_ON_NEW", cpkN, cpBool, &_sched_on_new,
		     cpEnd) < 0)
	return -1;

    if (__builtin_popcount(_que_len>>1) != 1) {
	errh->fatal("PushBatchQueue queue length %d not"
		    "power of 2", _que_len);
	return -1;
    }
    
    return 0;
}

int
PushBatchQueue::initialize(ErrorHandler *errh)
{
    if (!_que.reserve(_que_len)) {
	errh->fatal("PushBatchQueue failed to reserve batch queue.\n");
	return -1;
    }
    
    ScheduleInfo::initialize_task(this, &_task, errh);
    return 0;
}

void
PushBatchQueue::push(int, Packet *)
{
    hvp_chatter("Error: PushBatchQueue's push should not be called!\n");
}

void
PushBatchQueue::bpush(int i, PBatch *pb)
{
    if (_que.full()) {
	_drops += pb->npkts;
	pb->kill();
	if (_test)
	    hvp_chatter("Batch %p killed\n",
			pb);
    }
    else {
	_que.add_new(pb);
	if (_sched_on_new)
	    _task.fast_reschedule();
    }
}

bool
PushBatchQueue::run_task(Task *task)
{
    while (!_que.empty()) {
	PBatch *pb = _que.oldest();
	bool done = false;

	if (pb->dev_stream) {
	    if (_block)
		g4c_stream_sync(pb->dev_stream);
	    else
		done = g4c_stream_done(pb->dev_stream)?true:false;
	} else
	    done = true;

	if (_block || done) {
	    _que.remove_oldest();
	    output(0).bpush(pb);
	    if (_test)
		hvp_chatter("Batch %p done at %s.\n", pb,
			    Timestamp::now().unparse().c_str());
	    if (!_process_all) {
		if (_fast_sched)
		    _task.fast_reschedule();
		return true;
	    }
	} else
	    break;
    }
    _task.reschedule();
    return false;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(PushBatchQueue)
ELEMENT_LIBS(-lg4c)
