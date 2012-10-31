#ifndef CLICK_PUSH_BATCH_QUEUE_HH
#define CLICK_PUSH_BATCH_QUEUE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/pbatch.hh>
#include <g4c.h>
CLICK_DECLS

class PushBatchQueue : public Element {
public:
	PushBatchQueue();
	~PushBatchQueue();
}

CLICK_ENDDECLS
#endif
