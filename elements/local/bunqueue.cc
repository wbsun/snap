#include <click/config.h>
#include "bunqueue.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/timestamp.hh>
#include <click/atomic.hh>
CLICK_DECLS

const int BUnqueue::DEFAULT_LEN = (int)(1<<3);

BUnqueue::BUnqueue()
{
    _que_len = DEFAULT_LEN;
    _drops = 0;
    _test = fasle;
    _q_prod_lock = 0;
    _q_cons_lock = 0;
}

BUnqueue::~BUnqueue()
{
}

int
BUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int ql = -1;
    
    if (cp_va_kparse(conf, this, errh,
		     "LENGTH", cpkN, cpInteger, &ql,
		     "TEST", cpkN, cpBool, &_test,
		     cpEnd) < 0)
	return -1;

    if (ql != -1) {
	_que_len = ql;
	if (__builtin_popcount(ql>>1) != 1) {
	    errh->fatal("BUnqueue queue length %d is not"
			"power of 2.", ql);
	    return -1;
	}
    }
    
    return 0;
}

int
BUnqueue::initialize(ErrorHandler *errh)
{
    if (!_que.reserve(_que_len)) {
        errh->fatal("BUnqueue failed to reserve batch queue.\n");
	return -1;
    }
    
    return 0;
}

void
BUnqueue::push(int, Packet* p)
{
    output(0).push(p);
}


Packet *
BUnqueue::pull(int)
{
    hvp_chatter("Error: BUnqueue's pull should not be called!\n");
}

void
BUnqueue::bpush(int i, PBatch *pb)
{
//    while(atomic_uint32_t::swap(_q_prod_lock, 1) == 1);

/*    if (pb->producer->test_mode >= BatchProducer::test_mode1) {
      _que.add_new(pb);
      return;
      } */
    
    if (unlikely(_que.full())) {
	_drops += pb->npkts;
	pb->kill();
    } else
	_que.add_new(pb);

//     click_compiler_fence();
    // _q_prod_lock = 0;
    
    if (_test)
	hvp_chatter("new batch %p added stream %d.\n",
		    pb, pb->dev_stream);
}

PBatch *
BUnqueue::bpull(int port)
{
    if (_que.empty()) {
	return 0;
    }
    else {
//	while(atomic_uint32_t::swap(_q_cons_lock, 1) == 1);
	
	PBatch *pb = _que.oldest();
/*	if (pb->host_mem == 0) {
	    if (_test)
		hvp_chatter("batch %p done\n", pb);
	    _que.remove_oldest();
	    return pb;
	}
*/
	
	if (pb->dev_stream == 0 || g4c_stream_done(pb->dev_stream)) {
	    _que.remove_oldest_with_wmb();

// 	    click_compiler_fence();
	    // _q_cons_lock = 0;

	    if (_test)
		hvp_chatter("batch %p done at %s\n", pb,
			    Timestamp::now().unparse().c_str());
	    return pb;
	} else {
	    _que.remove_oldest();
// 	    click_compiler_fence();
	    // _q_cons_lock = 0;
	    pb->kill();
	    return 0;
	}
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BUnqueue)
ELEMENT_LIBS(-lg4c)
