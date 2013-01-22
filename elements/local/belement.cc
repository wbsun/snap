#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include "belement.hh"

CLICK_DECLS

BElement::BElement()
{
}

BElement::~BElement()
{
}

void
BElement::bpush(int i, PBatch *p)
{
    Element::bpush(i, p);
}

PBatch*
BElement::bpull(int i)
{
    return Element::bpull(i);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BElement)
