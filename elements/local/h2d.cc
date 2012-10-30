#include <click/config.h>
#include "h2d.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
CLICK_DECLS

H2D::H2D()
{
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
}

int
H2D::configure(Vector<String> &conf, ErrorHandler *errh)
{
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
