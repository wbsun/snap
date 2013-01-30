#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <arpa/inet.h>
#include <sys/time.h>
#include "bidsmatcher.hh"
CLICK_DECLS

BIDSMatcher::BIDSMatcher() : _test(0),
			     _batcher(0)
{
    _anno_offset = -1;
    _slice_offset = -1;
    _on_cpu = 0;
    _rand_pattern_max_len = 32;
}

BIDSMatcher::~BIDSMatcher()
{
}

int
BIDSMatcher::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _div = 0;
    if (cp_va_kparse(conf, this, errh,
		     "BATCHER", cpkN, cpElementCast, "Batcher", &_batcher,
		     "TEST", cpkN, cpInteger, &_test,
		     "DIV", cpkN, cpInteger, &_div,
		     "PTNLEN", cpkN, cpInteger, &_rand_pattern_max_len,
		     "CPU", cpkN, cpInteger, &_on_cpu, // 0: GPU, 1: CPU+batch, 2: CPU no batch
		     cpEnd) < 0)
	return -1;

    if (_on_cpu == 3) {
	errh->message("Skip mode.");
	return 0;
    }
    
    if (_on_cpu < 2)
	if (_batcher->req_anno(0, 4, BatchProducer::anno_write)) {
	    errh->error("Register annotation request in batcher failed");
	    return -1;
	}

    _psr.start = EthernetBatchProducer::udp4_payload; //42
    _psr.start_offset = 0; // TTL
    _psr.len = 18;
    _psr.end = _psr.start+_psr.start_offset+_psr.len;

    if (_on_cpu < 2)
	if (_batcher->req_slice_range(_psr) < 0) {
	    errh->error("Request slice range failed: %d, %d, %d, %d",
			_psr.start, _psr.start_offset, _psr.len, _psr.end);
	    return -1;
	}

    if (_test < 2) {
	errh->error("For now, we need random generated patterns, use a TEST"
		    " larger than 2 to assign #ptns");
	return -1;
    }

    return 0;
}

char**
BIDSMatcher::generate_random_patterns(int np, int plen)
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    srandom((unsigned)(tv.tv_usec));

    size_t tsz = np*sizeof(char*) + np*(plen);

    char *p = (char*)malloc(tsz);
    if (!p)
	return 0;

    char *ptn = p+np*sizeof(char*);
    char **pp = (char**)p;

    for (int i=0; i<np; i+=8) {
	pp[i] = ptn;
	int j;
	int mylen = (random()%(plen-4)) + 3;
	for (j=0; j<mylen; j++)
	    ptn[j] = (char)(random()%40 + 'A');
	ptn[j] = (char)0;
	ptn += plen;

	// simulating common prefix:
	for (int k=1; k<=7; k++) {
	    if (i+k == np)
		return pp;
	    
	    pp[i+k] = ptn;
	    for (j=0; j<mylen-1; j++)
		ptn[j] = pp[i][j];
	    ptn[j] = (char)(random()%40+'A');
	    ptn[j+1] = (char)0;
	    ptn += plen;
	}
    }

    return pp;   
}

int
BIDSMatcher::initialize(ErrorHandler *errh)
{
    if (_on_cpu == 3)
	return 0;
    
    char **ptns = 0;
    int nptns = 0;
    if (_test > 2) {
	ptns = generate_random_patterns(_test, _rand_pattern_max_len);
	if (!ptns) {
	    errh->error("Failed to alloc mem for patterns");
	    return -1;
	}
	nptns = _test;
    }
    
    int s = g4c_alloc_stream();
    if (!s) {
	errh->error("Failed to alloc stream for IDS matcher copy");
	return -1;
    }

    _acm = g4c_create_matcher(ptns, nptns, 1, s);
    if (!_acm || !_acm->devmem) {
	errh->error("Failed to create acm");
	if (_test > 2) {
	    g4c_free_stream(s);
	    free(ptns);
	}
	return -1;
    } else {
	errh->message("ACM built for host and device.");
    }

    g4c_free_stream(s);

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
	    errh->message("BIDSMatcher anno offset %d", _anno_offset);

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
	    errh->message("BIDSMatcher slice offset %d", _slice_offset);
    } else {
	_anno_offset = 0;
	_slice_offset = EthernetBatchProducer::udp4_payload;
    }
	
    return 0;
}

void
BIDSMatcher::bpush(int i, PBatch *p)
{
    if (_on_cpu <= 0) {
	if (!p->dev_stream)
	    p->dev_stream = g4c_alloc_stream();

	//if (input(i).element()->noutputs() == input(i).element()->ninputs())
	if (!_div) {
	    g4c_gpu_acm_match(
		(g4c_acm_t*)_acm->devmem,
		p->npkts,
		p->dslices(),
		p->producer->get_slice_stride(),
		_slice_offset,
		0,
		(int*)p->dannos(),
		p->producer->get_anno_stride()/sizeof(int),
		_anno_offset/sizeof(int),
		p->dev_stream, 0);
	}
	else if (_div==1) {
	    g4c_gpu_acm_match(
		(g4c_acm_t*)_acm->devmem,
		p->npkts,
		p->dslices(),
		p->producer->get_slice_stride(),
		_slice_offset,
		0,
		(int*)p->dannos(),
		p->producer->get_anno_stride()/sizeof(int),
		_anno_offset/sizeof(int),
		p->dev_stream, 3);
	}
	else {
	    g4c_gpu_acm_match(
		(g4c_acm_t*)_acm->devmem,
		p->npkts,
		p->dslices(),
		p->producer->get_slice_stride(),
		_slice_offset,
		0,
		(int*)p->dannos(),
		p->producer->get_anno_stride()/sizeof(int),
		_anno_offset/sizeof(int),
		p->dev_stream, 4);
	}
	    
	p->hwork_ptr = p->hannos();
	p->dwork_ptr = p->dannos();
	p->work_size = p->npkts * p->producer->get_anno_stride();
    } else {
	if (_on_cpu == 3)
	    goto getout;
	if (_on_cpu < 2) {
	    for(int j=0; j<p->npkts; j++) {
		*(int*)(g4c_ptr_add(p->anno_hptr(j), _anno_offset)) =
		    g4c_cpu_acm_match(
			_acm,
			(uint8_t*)g4c_ptr_add(p->slice_hptr(j), _slice_offset),
			p->producer->get_slice_stride() - _slice_offset);
	    }
	} else {
	    for (int j=0; j<p->npkts; j++) {
		Packet *pkt = p->pptrs[j];
		*(int*)(g4c_ptr_add(pkt->anno(), _anno_offset)) =
		    g4c_cpu_acm_match(_acm,
				      (uint8_t*)g4c_ptr_add(pkt->data(), _slice_offset),
				      pkt->length()-_slice_offset);
	    }
	}
    }
getout:
    output(i).bpush(p);
}

void
BIDSMatcher::push(int i, Packet *p)
{
    if (_on_cpu != 3) {
	*(int*)(g4c_ptr_add(p->anno(), _anno_offset)) =
	    g4c_cpu_acm_match(_acm,
			      (uint8_t*)g4c_ptr_add(p->data(), _slice_offset),
			      p->length()-_slice_offset);
    }
    output(i).push(p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BElement)
EXPORT_ELEMENT(BIDSMatcher)
ELEMENT_LIBS(-lg4c)    
