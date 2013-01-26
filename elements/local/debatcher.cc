#include <click/config.h>
#include "debatcher.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/hvputils.hh>
CLICK_DECLS

DeBatcher::DeBatcher()
{
    _batch = 0;
    _idx = 0;
    _color = -1;
    _anno = 0;
}

DeBatcher::~DeBatcher()
{
    if (_batch)
	_batch->kill();
}

int
DeBatcher::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
		     "COLOR", cpkP, cpInteger, &_color,
		     "ANNO", cpkN, cpInteger, &_anno,
		     cpEnd) < 0)
	return -1;

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
    output(0).push(p);
}


Packet *
DeBatcher::pull(int port)
{
    if (!_batch) {
    pull_batch:
	_batch = input(0).bpull();
	if (!_batch)
	    return 0;

#ifndef CLICK_NO_BATCH_TEST
	if (_batch->producer->test_mode >= BatchProducer::test_mode1)
	    goto _try_kill_batch;
#endif
	if (_batch->dev_stream) {
	    g4c_free_stream(_batch->dev_stream);
	    _batch->dev_stream = 0;
	}

    _try_kill_batch:
	if (_batch->npkts == 0) {
	    _batch->kill();
	    goto pull_batch;
	}

	_idx = 0;
	if (_batch->producer->has_annos() &&
	    _batch->producer->anno_len > _anno+4) {
	    int i;
	    for (i=0; i<_batch->npkts; i++) {
		if ((*(int*)g4c_ptr_add(
			 _batch->anno_hptr(i), _anno)) != _color)
		    continue;
		else {
		    _idx = i;
		    break;
		}
	    }

	    if (i==_batch->npkts)
		goto pull_batch;
	}	
    }

    Packet *p = _batch->pptrs[_idx++];
//     assert(p);

    if (_idx == _batch->npkts) {
    release_batch:
	//_batch->npkts = 0;
	_batch->kill();
	_batch = 0;
    } else if (_batch->producer->has_annos() &&
	       _batch->producer->anno_len > _anno+4) {
	int ii;
	for (ii=_idx; ii<_batch->npkts; ii++) {
	    if ((*(int*)g4c_ptr_add(
		     _batch->anno_hptr(ii), _anno)) != _color)
		continue;
	    else {
		_idx = ii;
		break;
	    }
	}

	if (ii==_batch->npkts)
	    goto release_batch;
    }

    return p;	
}

void
DeBatcher::bpush(int i, PBatch *pb)
{
#ifndef CLICK_NO_BATCH_TEST
    if (pb->producer->test_mode >= BatchProducer::test_mode1)
	goto _push_pkts;
#endif
    
    if (pb->dev_stream) {
	g4c_free_stream(pb->dev_stream);
	pb->dev_stream = 0;
    }

_push_pkts:
    for (int j = 0; j < pb->npkts; j++) {
	if (pb->producer->has_annos() && pb->producer->anno_len > _anno+4) {
	    if ((*(int*)g4c_ptr_add(pb->anno_hptr(j), _anno)) != _color)
		continue;
	}
	output(0).push(pb->pptrs[j]);
    }
    //pb->npkts = 0;
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
