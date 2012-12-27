#include <click/config.h>
#include "h2d.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/hvputils.hh>
CLICK_DECLS

H2D::H2D()
{
    _test = false;
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
	if (unlikely(_test))
	    hvp_chatter("No copy\n");
	output(0).bpush(pb);
	return;
    }

    if (pb->dev_stream == 0) {
	pb->dev_stream = g4c_alloc_stream();
	if (pb->dev_stream == 0) {
	    if (_test) {
		hvp_chatter(
		    "Drop pbatch %p because of stream shortage\n", pb);
	    }
	    drop_batch(pb);
	    return;
	}
    }

    g4c_h2d_async(pb->hwork_ptr, pb->dwork_ptr,
		  pb->work_size, pb->dev_stream);
    if (unlikely(_test))
	hvp_chatter("Copied %d bytes\n", pb->work_size);

    output(0).bpush(pb);
}

void
H2D::drop_batch(PBatch *pb)
{
    pb->kill();
}

int
H2D::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
		     "TEST", cpkN, cpBool, &_test,
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
EXPORT_ELEMENT(H2D)
ELEMENT_LIBS(-lg4c)
