#include <click/config.h>
#include "batcher.hh"
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

Batcher::Batcher()
{
	_count = 0;
	_batch_capacity = CLICK_PBATH_CAPACITY;
	_cur_batch_size = 0;
	_batch = 0;
	_slice_begin = 0;
	_slice_end = 0;
	_anno_flags = 0;
}

Batcher::~Batcher()
{
}

PBatch*
Batcher::alloc_batch()
{
	_batch = new PBatch(_batch_capacity, _slice_begin, _slice_end,
			    _anno_flags, Packet::anno_size);
	if (_batch) {
		_batch->init_for_host_batching();
		_batch->hostmem = g4c_alloc_page_lock_mem(_batch->memsize);
		_batch->devmem = g4c_alloc_dev_mem(_batch->memsize);
		_batch->set_pointers();
		_batch->hwork_ptr = _batch->hostmem;
		_batch->dwork_ptr = _batch->devmem;
		_batch->work_size = _batch->memsize;
		_batch->work_data = 0;	
	}
	
	return _batch;
}

/**
 * Pre-condition: _batch exists, _batch not full. Packet p checked.
 */
void
Batcher::add_packet(Packet *p)
{
	int idx = _batch->size;

	_batch->size++;
	_batch->pptrs[idx] = p;
	// TODO: copy slice data from p to _batch's slice.
}

void
Batcher::push(int i, Packet *p)
{
	if (!_batch)
		alloc_batch();

	add_packet(p);
	_count++;
	
	if (_batch->full()) {
		PBatch *oldbatch = _batch;
		alloc_batch();
		output(0).bpush(oldbatch);
	}
}

int
Batcher::configure(Vector<String> &conf, ErrorHandler *errh)
{
}

int
Batcher::initialize(ErrorHandler *errh)
{
}

void
Batcher::run_timer(Timer *timer)
{
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
Batcher::set_anno_flags(unsigned int flags)
{
	_anno_flags |= flags;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Batcher)
