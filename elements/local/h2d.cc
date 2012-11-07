#include <click/config.h>
#include "h2d.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "batcher.hh"
CLICK_DECLS

H2D::H2D()
{
	_clear_pktflags = false;
}

H2D::~H2D()
{
}

void
H2D::push(int i, Packet *p)
{
	hvp_chatter("Error: H2D's push should not be called!\n");
}

void
H2D::bpush(int i, PBatch *pb)
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
			drop_batch(pb);
			return;
		}
	}

	g4c_h2d_async(pb->hwork_ptr, pb->dwork_ptr, pb->work_size, pb->dev_stream);
	if (_clear_pktflags)
		g4c_dev_memset(pb->dpktflags, 0, pb->capacity*sizeof(unsigned int),
			       pb->dev_stream);
	output(0).bpush(pb);
}

void
H2D::drop_batch(PBatch *pb)
{
	Batcher::kill_batch(pb);	
}

int
H2D::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh,
			 "CLEAR_PKTFLAGS", cpkN, cpBool, &_clear_pktflags,
			 cpEnd) < 0)
		return -1;
	return 0;
}

int
H2D::initialize(ErrorHandler *errh)
{
	return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Batcher)
EXPORT_ELEMENT(H2D)
ELEMENT_LIBS(-lg4c)
