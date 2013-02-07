#include <click/config.h>
#include "d2h.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "gpuruntime.hh"
CLICK_DECLS

D2H::D2H()
{
}

D2H::~D2H()
{
}

void
D2H::push(int i, Packet *p)
{
    output(i).push(p);
}

void
D2H::bpush(int i, PBatch *pb)
{
    if (pb->work_size == 0
#ifndef CLICK_NO_BATCH_TEST
	|| pb->producer->test_mode >= BatchProducer::test_mode1
#endif
	) {
	output(i).bpush(pb);
	return;
    }

    if (pb->dev_stream == 0) {
	pb->dev_stream = g4c_alloc_stream();
	if (pb->dev_stream == 0) {
	    pb->kill();
	    return;
	}
    }

    g4c_d2h_async(pb->dwork_ptr, pb->hwork_ptr,
		  pb->work_size, pb->dev_stream);
    output(i).bpush(pb);
}

int
D2H::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return 0;
}

int
D2H::initialize(ErrorHandler *errh)
{
    return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BElement GPURuntime)
EXPORT_ELEMENT(D2H)
ELEMENT_LIBS(-lg4c)
