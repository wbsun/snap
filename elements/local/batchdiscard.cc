
#include <click/config.h>
#include "batchdiscard.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/hvputils.hh>
CLICK_DECLS

BatchDiscard::BatchDiscard()
    : _task(this), _count(0), _burst(1), _active(true)
{
}

int
BatchDiscard::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read("ACTIVE", _active)
	.read("BURST", _burst)
	.complete() < 0)
	return -1;
    if (!_active && input_is_push(0))
	return errh->error("ACTIVE is meaningless in push context");
    if (_burst == 0)
	_burst = ~(unsigned) 0;
    return 0;
}

int
BatchDiscard::initialize(ErrorHandler *errh)
{
    if (input_is_pull(0)) {
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    }
    return 0;
}

void
BatchDiscard::bpush(int, PBatch *p)
{
    _count++;
    p->producer->kill_batch(p);    
}

void
BatchDiscard::push(int, Packet *p)
{
    hvp_chatter("Should not call this: Packet %p\n", p);
    p->kill();
}

bool
BatchDiscard::run_task(Task *)
{
    unsigned x = _burst;
    PBatch *p;
    while (x && (p = input(0).bpull())) {
	p->producer->kill_batch(p);	
	--x;
    }
    unsigned sent = _burst - x;

    _count += sent;
    if (_active || sent)
	_task.fast_reschedule();
    return sent != 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatchDiscard)
ELEMENT_MT_SAFE(BatchDiscard)
