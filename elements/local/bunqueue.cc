#include <click/config.hh>
#include "bunqueue.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "batcher.hh"
CLICK_DECLS

const int BUnqueue::DEFAULT_LEN = (int)(1<<16);

BUnqueue::BUnqueue()
{
	_que_len = DEFAULT_LEN;
}

BUnqueue::~BUnqueue()
{
}

int
BUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh,
			 "LENGTH", cpkN, cpInteger, &_que_len,
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
	_que.push_back(pb);
}

PBatch *
BUnqueue::bpull(int port)
{
	if (_que.empty())
		return 0;
	else {
		PBatch *pb = _que.front();
		if (g4c_stream_done(pb->dev_stream)) {
			_que.pop_front();
			return pb;
		} else
			return 0;
	}
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(Batcher)
EXPORT_ELEMENT(Bunqueue)
ELEMENT_LIBS(-lg4c)
