#include <click/config.hh>
#include "pushbatchqueue.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "batcher.hh"
CLICK_DECLS

const int PushBatchQueue::DEFAULT_LEN = (int)(1<<16);

PushBatchQueue::PushBatchQueue() : _task(this), _que_len(DEFAULT_LEN),
				   _block(false), _process_all(false),
				   _fast_sched(false)
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
			 cpEnd) < 0)
		return -1;
	return 0;
}

int
PushBatchQueue::initialize(ErrorHandler *errh)
{
	_que.reserve(_que_len);
	ScheduleInfo::initialize_task(this, &_task, errh);
	return 0;
}

void
PushBatchQueue::push(int i, Packet *p)
{
	hvp_chatter("Error: PushBatchQueue's push should not be called!\n");
}

void
PushBatchQueue::bpush(int i, PBatch *pb)
{
	if (!_que.add_new(pb)) {
		_drops += pb->size();
		Batcher::kill_batch(pb);
	}
}

bool
PushBatchQueue::run_task(Task *task)
{
	while (!_que.empty()) {
		PBatch *pb = _que.oldest();
		bool done = false;

		if (_block)
			g4c_stream_sync(pb->dev_stream);
		else
			done = g4c_stream_done(pb->dev_stream)?true:false;

		if (_block || done) {
			_que.remove_oldest();
			output(0).bpush(pb);
			if (!_process_all) {
				if (_fast_sched)
					_task.fast_reschedule();
				return true;
			}
		} else
			break;
	}
	return false;
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(Batcher)
EXPORT_ELEMENT(PushBatchQueue)
ELEMENT_LIBS(-lg4c)
