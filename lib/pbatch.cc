#include <click/pbatch.hh>
//#include <g4c.h>

CLICK_DECLS

PBatch::PBatch(): capacity(CLICK_PBATCH_CAPACITY), size(0), pptrs(0),
		  hostmem(0), devmem(0), hpktlens(0), hslices(0), hpktannos(0),
		  dpktlens(0), dslices(0), dpktannos(0), slice_begin(0),
		  slice_end(0), slice_length(0), slice_size(0),
		  anno_flags(0), anno_size(0), dev_stream(0),
		  hwork_ptr(0), dwork_ptr(0), work_size(0)
{
}

PBatch::~PBatch() {}

int
PBatch::init_for_host_batching()
{
	pptrs = new Packet*[capacity];
	if (!pptrs)
		return -1;
	
	memset(pptrs, 0, sizeof(Packet*)*capacity);
	return 0;
}

void
PBatch::clean_for_host_batching()
{
	delete[] pptrs;
}

CLICK_ENDDECLS
