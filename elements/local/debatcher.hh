#ifndef CLICK_DEBATCHER_HH
#define CLICK_DEBATCHER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/pbatch.hh>
#include <g4c.h>
CLICK_DECLS

class DeBatcher : public Element {
public:
	DeBatcher();
	~DeBatcher();
}

CLICK_ENDDECLS
#endif
