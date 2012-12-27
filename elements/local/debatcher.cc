#include <click/config.h>
#include "debatcher.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
CLICK_DECLS

DeBatcher::DeBatcher()
{
    _batch = 0;
    _idx = 0;
}

DeBatcher::~DeBatcher()
{
    if (_batch)
	_batch->kill();
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

	if (_batch->dev_stream) {
	    g4c_free_stream(_batch->dev_stream);
	    _batch->dev_stream = 0;
	}
	
	if (_batch->npkts == 0) {
	    _batch->kill();
	    goto pull_batch;
	}
	_idx = 0;
    }

    Packet *p = _batch->pptrs[_idx++];
//     assert(p);

    if (_idx == _batch->npkts) {
	_batch->npkts = 0;
	_batch->kill();
	_batch = 0;
    }

    return p;	
}

void
DeBatcher::bpush(int i, PBatch *pb)
{
    if (pb->dev_stream) {
	g4c_free_stream(pb->dev_stream);
	pb->dev_stream = 0;
    }
    
    for (int j = 0; j < pb->npkts; j++)
	output(0).push(pb->pptrs[j]);
    pb->npkts = 0;
    pb->kill();
}

PBatch *
DeBatcher::bpull(int port)
{
    hvp_chatter("Error: DeBatcher's bpull should not be called!\n");
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DeBatcher)
ELEMENT_LIBS(-lg4c)
