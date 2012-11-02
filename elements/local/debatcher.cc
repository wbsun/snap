#include <click/config.h>
#include "debatcher.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "batcher.hh"
CLICK_DECLS

DeBatcher::DeBatcher()
{
	_batch = 0;
	_idx = 0;
}

DeBatcher::~DeBatcher()
{
	if (_batch)
		Batcher::kill_batch(_batch);
}

int
DeBatcher::configure(Vector<String> &conf, ErrorHandler *errh)
{
	return 0;
}

int
DeBatcher::initialize(ErrorHandler *errh)
{
	return 0;
}

void
DeBatcher::push(int i, Packet *p)
{
	hvp_chatter("Error: DeBatcher's push should not be called!\n");
}


Packet *
DeBatcher::pull(int port)
{
	if (!_batch) {
	pull_batch:
		_batch = input(0).bpull();
		if (!_batch)
			return 0;
		if (_batch->size() == 0) {
			Batcher::kill_batch(_batch);
			goto pull_batch;
		}
		_idx = 0;
	}

	Packet *p = _batch->pptrs[_idx++];
	assert(p);

	if (_idx == _batch->size()) {
		Batcher::kill_batch(_batch);
		_batch = 0;
	}

	return p;	
}

void
DeBatcher::bpush(int i, PBatch *pb)
{
	for (int j = 0; j < pb->size(); j++)
		output(0).push(pb->pptrs[j]);
	Batcher::kill_batch(pb);
}

PBatch *
DeBatcher::bpull(int port)
{
	hvp_chatter("Error: DeBatcher's bpull should not be called!\n");
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Batcher)
EXPORT_ELEMENT(DeBatcher)
ELEMENT_LIBS(-lg4c)
