#include <click/config.h>
#include "batcher.hh"
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/pbatch.hh>
CLICK_DECLS

Batcher::Batcher(): _timer(this)
{
	_count = 0;
	_drops = 0;
	_batch_capacity = CLICK_PBATCH_CAPACITY;
	_cur_batch_size = 0;
	_batch = 0;
	_slice_begin = 0;
	_slice_end = 0;
	_anno_flags = 0;
	_timeout_ms = CLICK_BATCH_TIMEOUT;
	_force_pktlens = false;
	_timed_batch = 0;
	_nr_users = 0;
	_user_priv_len = 0;
}

Batcher::~Batcher()
{
}

PBatch*
Batcher::alloc_batch()
{
	_batch = new PBatch(_batch_capacity, _slice_begin, _slice_end, _force_pktlens,
			    _anno_flags, Packet::anno_size);
	if (_batch) {
		_batch->init_for_host_batching();
		_batch->hostmem = g4c_alloc_page_lock_mem(_batch->memsize);
		_batch->devmem = g4c_alloc_dev_mem(_batch->memsize);
		_batch->set_pointers();

		// TODO:
		//   A fater option is to not copy flags, lunching a kernel
		//   to init device size pktflags or using cudaMemset.
		_batch->hwork_ptr = _batch->hostmem; 
		_batch->dwork_ptr = _batch->devmem;
		_batch->work_size = _batch->memsize;

		_batch->nr_users = _nr_users;
		_batch->user_priv_len = _user_priv_len;
		_batch->user_priv = malloc(_user_priv_len);
		if (!_batch->user_priv) {
			hvp_chatter("Out of memory.\n");
			kill_batch(_batch);
			_batch = 0;
		}
	}

	_cur_batch_size = 0;
	
	return _batch;
}

bool
Batcher::kill_batch(PBatch *pb)
{
	pb->shared--;
	if (pb->shared < 0) {
		g4c_free_page_lock_mem(pb->hostmem, pb->memsize);
		g4c_free_dev_mem(pb->devmem, pb->memsize);

		g4c_free_stream(pb->dev_stream);
		free(pb->user_priv);
		delete pb;
		return true;
	}

	return false;
}

/**
 * Pre-condition: _batch exists, _batch not full. Packet p checked.
 */
void
Batcher::add_packet(Packet *p)
{
	int idx = _batch->size();

	if (p->has_mac_header()) {
		_batch->npkts++;
		_batch->pptrs[idx] = p;
		_cur_batch_size = _batch->size();

		unsigned long copysz = p->end_data() - p->mac_header();
		if (_batch->slice_end > 0 && copysz > (unsigned long)_batch->slice_length)
			copysz = _batch->slice_length;

		if (_batch->hpktlens) {
			*_batch->hpktlen(idx) = copysz;
		}

		_batch->hpktflags = 0;
		memcpy(_batch->hslice(idx),
		       p->mac_header()+_batch->slice_begin,
		       copysz);
		
		if (_batch->anno_flags & PBATCH_ANNO_READ) {
			memcpy(_batch->hanno(idx),
			       p->anno(),
			       Packet::anno_size);
		}

		if (idx == 0 && _timeout_ms > 0) {			
			_timer.schedule_after_msec(_timeout_ms);
			_timed_batch = _batch;
		}
	} else {
		hvp_chatter("add_packet(): packet has no MAC header\n");
		_drops++;
	}	
}

void
Batcher::push(int i, Packet *p)
{
	if (!_batch) {
		alloc_batch();		
	}

	add_packet(p);
	_count++;
	
	if (_batch->full()) {
		if (_timer.scheduled())
			_timer.clear();
		PBatch *oldbatch = _batch;
		alloc_batch();
		output(0).bpush(oldbatch);
	}
}

int
Batcher::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh,
			 "TIMEOUT", cpkN, cpInteger, &_timeout_ms,
			 "SLICE_BEGIN", cpkN, cpInteger, &_slice_begin,
			 "SLICE_END", cpkN, cpInteger, &_slice_end,
			 "CAPACITY", cpkN, cpInteger, &_batch_capacity,
			 "ANN_FLAGS", cpkN, cpByte, &_anno_flags,
			 "FORCE_PKTLENS", cpkN, cpBool, &_force_pktlens,
			 cpEnd) < 0)
		return -1;
	return 0;
}

int
Batcher::initialize(ErrorHandler *errh)
{
	_timer.initialize(this);
	return 0;
}

void
Batcher::run_timer(Timer *timer)
{
	if (_timed_batch != _batch || !_timed_batch)
		return;
	// NOT READY FOR TIMEPUT OPS YET.
}

void
Batcher::set_slice_range(int begin, int end)
{
	if (begin < _slice_begin)
		_slice_begin = begin;

	if (end < 0) {
		_slice_end = -1;
		return;
	}

	if (end > _slice_end)
		_slice_end = end;
}

void
Batcher::set_anno_flags(unsigned char flags)
{
	_anno_flags |= flags;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Batcher)
ELEMENT_LIBS(-lg4c)

