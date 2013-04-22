#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <arpa/inet.h>
#include <sys/time.h>
#include <click/atomic.hh>
#include "gpclassifier.hh"

CLICK_DECLS

GPClassifier::GPClassifier() : _test(0),
			     _batcher(0)
{
    _anno_offset = -1;
    _slice_offset = -1;
    _on_cpu = 0;
    _classifier = 0;
}

GPClassifier::~GPClassifier()
{
}

int
GPClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _div = false;
    if (cp_va_kparse(conf, this, errh,
		     "BATCHER", cpkN, cpElementCast, "Batcher", &_batcher,
		     "CLASSIFIER", cpkM, cpElementCast, "ClassifierRuleset", &_classifier,
		     "TEST", cpkN, cpInteger, &_test,
		     "DIV", cpkN, cpBool, &_div,		     
		     "CPU", cpkN, cpInteger, &_on_cpu, // 0: GPU, 1: CPU+batch, 2: CPU no batch
		     cpEnd) < 0)
	return -1;

    if (_on_cpu == 3) {
	errh->message("Skip mode.");
	return 0;
    }
    
    if (_on_cpu < 2)
	if (_batcher->req_anno(0, 1, BatchProducer::anno_write)) {
	    errh->error("Register annotation request in batcher failed");
	    return -1;
	}

    _psr.start = EthernetBatchProducer::ip4_hdr; // 14
    _psr.start_offset = 8; // TTL
    _psr.len = 16;
    _psr.end = _psr.start+_psr.start_offset+_psr.len;

    if (_on_cpu < 2)
	if (_batcher->req_slice_range(_psr) < 0) {
	    errh->error("Request slice range failed: %d, %d, %d, %d",
			_psr.start, _psr.start_offset, _psr.len, _psr.end);
	    return -1;
	}
    
    return 0;
}

int
GPClassifier::initialize(ErrorHandler *errh)
{
    if (_on_cpu == 3)
	return 0;

    if (_on_cpu < 2) {
	_batcher->setup_all();
	_anno_offset = _batcher->get_anno_offset(0);
	if (_anno_offset < 0) {
	    errh->error("Failed to get anno offset in batch "
			"anno start %u, anno len %u "
			"w start %u, w len %u",
			_batcher->anno_start, _batcher->anno_len,
			_batcher->w_anno_start, _batcher->w_anno_len);
	    return -1;
	} else
	    errh->message("GPClassifier anno offset %d", _anno_offset);

	_slice_offset = _batcher->get_slice_offset(_psr);
	if (_slice_offset < 0) {
	    errh->error("Failed to get slice offset in batch ranges:");
	    for (int i=0; i<_batcher->nr_slice_ranges; i++) {
		errh->error("start %d, off %d, len %d, end %d",
			    _batcher->slice_ranges[i].start,
			    _batcher->slice_ranges[i].start_offset,
			    _batcher->slice_ranges[i].len,
			    _batcher->slice_ranges[i].end);
	    }
	    return -1;
	} else
	    errh->message("GPClassifier slice offset %d", _slice_offset);
    } else {
	_anno_offset = 0;
	_slice_offset = 22; // IP dst
    }
	
    return 0;
}

void
GPClassifier::bpush(int i, PBatch *p)
{
    if (!_on_cpu && !p->dev_stream)
	p->dev_stream = g4c_alloc_stream();
    
    if (likely(_classifier->classify_packets(
		   p, _anno_offset, _slice_offset, _on_cpu) == 0)) {
	p->hwork_ptr = p->hannos();
	p->dwork_ptr = p->dannos();
	p->work_size = p->npkts * p->producer->get_anno_stride()/sizeof(int);
    } else {
	hvp_chatter("call classifier failed\n");
    }	
getout:
    if (!_div)
	output(i).bpush(p);
    else {
	atomic_uint32_t::inc(p->shared);
	output(i*2).bpush(p);
	output(i*2+1).bpush(p);
    }	
}

void
GPClassifier::push(int i, Packet *p)
{
    if (_on_cpu < ClassifierRuleset::CLASSIFY_CPU_NON_BATCH) {
	hvp_chatter("Should never call this: %d, %p\n", i, p);
    }
    else {
	_classifier->classify_packet(p, anno_ofs, slice_ofs, _on_cpu);	
	output(i).push(p);
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BElement ClassifierRuleset)
EXPORT_ELEMENT(GPClassifier)
ELEMENT_LIBS(-lg4c)    
