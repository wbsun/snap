#include <click/config.h>
#include "d2h.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "batcher.hh"
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
	hvp_chatter("Error: D2H's push should not be called!\n");
}

void
D2H::bpush(int i, PBatch *pb)
{
	if (pb->work_size == 0 ||
	    pb->hwork_ptr == 0 ||
	    pb->dwork_ptr == 0) {
		output(0).bpush(pb);
		return;
	}

	if (pb->dev_stream == 0) {
		pb->dev_stream = g4c_alloc_stream();
		if (pb->dev_stream == 0) {
			Batcher::kill_batch(pb);
			return;
		}
	}

	g4c_d2h_async(pb->dwork_ptr, pb->hwork_ptr, pb->work_size, pb->dev_stream);
	output(0).bpush(pb);
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
ELEMENT_REQUIRES(Batcher)
EXPORT_ELEMENT(D2H)
ELEMENT_LIBS(-lg4c)
