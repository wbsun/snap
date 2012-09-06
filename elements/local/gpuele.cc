#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

GPUEle::GPUEle()
{
	_count = 0;
	_pktbufsz = DEFAULT_PACKET_BUF_SZ;
	_batchsz = DEFAULT_BATCH_SZ;
	_ustimeout = DEFAULT_BATCH_TIMEOUT;
	_task = 0;
}

GPUEle::~GPUEle()
{
}

int
GPUEle::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh,
			 "BATCHSZ", 0, cpUnsigned, &_batchsz,
			 "PKTBUFSZ", 0, cpSize, &_pktbufsz,
			 "TIMEOUT", 0, cpUnsigned, &_ustimeout,
			 cpEnd) < 0) return -1;
	if (_pktbufsz < 1540) // Need a real common MTU to figure out this
		return errh->error("packet buffer size is %u, too small\n", _pktbufsz);

	return 0;
}

PacketBatch
GPUEle::new_batch()
{
	PacketBatch pb;
	pb.first = new PacketQueue();
	if (pb.first) {
		if (pb.first->reserve(_batchsz)) {
			pb.second = g4c_malloc(_batchsz*_pktbufsz);
			if (pb.second)
				return pb;
			
			click_chatter("g4c malloc failed, sz %uB\n", _batchsz*_pktbufsz);
		} else
			click_chatter("can't reserve %d items space for packet batch\n", _batchsz);

		delete pb.first;
		pb.first = 0;
	} else
		click_chatter("new packet queue allocation failed\n");

	return pb;
}

void
GPUEle::destroy_batch(PacketBatch pb)
{
	delete pb->first;
	g4c_free(pb->second);
}

bool
GPUEle::init_batch_pool()
{
	int nrpool = MAX_NR_BATCH + 10;
	int i;

	if (_batchpool.reserve(nrpool)) {
		for (i=0; i<nrpool; i++) {
			PacketBatch pb = new_batch();
			if (pb.first)
				_batchpool.push_back(pb);
			else {
				click_chatter("the %d batch failed to alloc\n", i);
			}
		}
	} else {
		click_chatter("batch pool faile to reserve space\n");
		return false;
	}

	return true;	
}

void
GPUEle::free_batch(PacketBatch pb)
{
	pb.first.clear();
	_batchpool.push_front(pb);
}

PacketBatch
GPUEle::alloc_batch()
{
	PacketBatch pb(0,0);

	if (!_batchpool.empty()) {
		pb = _batchpool.back();
		_batchpool.pop_back();
	}

	return pb;
}

void
GPUEle::free_batch_pool()
{
	int i;

	for(i=0; i<_batchpool.size(); i++)
		destroy_batch(_batchpool[i]);

	_batchpool.clear();
}

int
GPUEle::initialize(ErrorHandler *errh)
{
	errh->message("initializing GPUEle...");

	if (!_todobatches.reserve(MAX_NR_BATCH)) {
		click_chatter("todo batches failed to reserve space\n");
		return -ENOMEM;
	}

	if (!_donebatches.reserve(MAX_NR_BATCH)) {
		click_chatter("done batches failed to reserve space\n");
		return -ENOMEM;
	}

	if (!init_batch()) {
		click_chatter("batch pool failed to initialize\n");
		free_batch_pool();
		return -ENOMEM;		
	}

	_curbatch = alloc_batch();
	if (!_curbatch.first)
		return -ENOMEM;

	_pullbatch.first = 0;
	_pullbatch.second = 0;

	ScheduleInfo::initialize_task(this, &_task, errh);
	
	return 0;
}

void
GPUEle::run_timer(Timer *timer)
{
}

bool
GPUEle::run_task(Task *task)
{
	if (!_todobatches.empty()) {
		/* Should acquire lock to _todobatches  */
		PacketBatch pb = _todobatches.back();
		_todobatches.pop_back();

		int i;
		for (i=0; i<pb.first->size(); i++)
			memcpy(pb.second + i*(_pktbufsz>>1), pb.first->at(i)->data(),
			       pb.first->at(i)->length());
		
		int err = g4c_do_stuff_sync(pb.second, pb.second+_batchsz*(_pktbufsz>>1), _batchsz);
		if (err) {
			click_chatter("GPU execution error: %s\n", g4c_strerror(err));
		} else {
			int *detectres = (int*)(pb.second+_batchsz*(_pktbufsz>>1));
			for (i=0; i<_batchsz; i++) {
				if (detectres[i]) 
					click_chatter("suspicious packet found @ %d\n", i);
			}
		}

		_donebatches.push_front(pb);
		return true;
	}

	return false;
}


Packet *
GPUEle::pull(int i)
{
	if (_pullbatch.first && _pullbatch.first->empty()) {
		if (_donebatches.empty()) {
			return 0;
		}

		free_batch(_pullbatch);

		_pullbatch = _donebatches.back();
		_donebatches.pop_back();
	}

	Packet *p = _pullbatch.first->back();
	_pullbatch.first->pop_back();
	
	return p;
}

void
GPUEle::push(int i, Packet *p)
{
	_count++;
check_batch_full:
	if (!_curbatch.first || _curbatch.first->size() == _batchsz)
	{
		if (_curbatch.first)
			_todobatches.push_front(_curbatch);
		
		_curbatch = alloc_batch();
		if (!_curbatch.first) {
			click_chatter("out of batch in pool\n");
			_dropped++;
			return;
		}
	}

	_curbatch.first->push_front(p);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(GPUEle)
