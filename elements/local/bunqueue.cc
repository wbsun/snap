#include <click/config.h>
#include "bunqueue.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "batcher.hh"
#include <click/confparse.hh>
#include <click/timestamp.hh>
#include <click/atomic.hh>
CLICK_DECLS

const int BUnqueue::DEFAULT_LEN = (int)(1<<16);

BUnqueue::BUnqueue()
{
    _que_len = DEFAULT_LEN;
    _drops = 0;
    _test = false;
    _qlock = 0;
}

BUnqueue::~BUnqueue()
{
}

int
BUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
		     "LENGTH", cpkN, cpInteger, &_que_len,
		     "TEST", cpkN, cpBool, &_test,
		     cpEnd) < 0)
	return -1;
    return 0;
}

int
BUnqueue::initialize(ErrorHandler *errh)
{
    _que.reserve(_que_len);
    return 0;
}

void
BUnqueue::push(int i, Packet *p)
{
    hvp_chatter("Error: BUnqueue's push should not be called!\n");
}


Packet *
BUnqueue::pull(int port)
{
    hvp_chatter("Error: BUnqueue's pull should not be called!\n");
}

void
BUnqueue::bpush(int i, PBatch *pb)
{
//    while(atomic_uint32_t::swap(_qlock, 1) == 1);
    if (!_que.add_new(pb)) {
	_drops += pb->size();
	Batcher::kill_batch(pb, true);
    }
    if (_test)
	hvp_chatter("new batch %p added stream %d.\n",
		    pb, pb->dev_stream);
    _qlock = 0;
}

PBatch *
BUnqueue::bpull(int port)
{
    if (_que.empty())
	return 0;
    else {
//	while(atomic_uint32_t::swap(_qlock, 1) == 1);
	PBatch *pb = _que.oldest();
	if (pb->dev_stream == 0 || g4c_stream_done(pb->dev_stream)) {
	    _que.remove_oldest();
	    if (_test)
		hvp_chatter("batch %p done at %s\n", pb,
			    Timestamp::now().unparse().c_str());
	    _qlock = 0;
	    return pb;
	} else {
	    return 0;
	    _qlock = 0;
	}
    }
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(Batcher)
EXPORT_ELEMENT(BUnqueue)
ELEMENT_LIBS(-lg4c)
