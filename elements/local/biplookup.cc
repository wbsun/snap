#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/ipaddress.hh>
#include <click/sync.hh>
#include <click/args.hh>
#include <arpa/inet.h>
#include "biplookup.hh"
CLICK_DECLS

BIPLookup::BIPLookup() : _test(false),
			 _hlpmt(0), _dlpmt(0),
			 _lpmt_lock(0),
			 _lpm_bits(4),
			 _lpm_size(0), _batcher(0)
{
    _anno_offset = -1;
    _slice_offset = -1;
    _on_cpu = 0;
}

BIPLookup::~BIPLookup()
{
}

int
BIPLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (!conf.size()) {
	errh->error("No routing entry");
	return -1;
    }

    int nargs;
    if (!cp_integer(conf[0], &nargs)) {
	errh->error("First argument must be integer to specify # of general args");
	return -1;
    }

    Vector<String> rts;
    Vector<String> myconf;

    myconf.reserve(nargs);
    rts.reserve(conf.size());

    int i;
    for (i=1; i<=nargs; i++)
	myconf.push_back(conf[i]);
    for (; i<conf.size(); i++)
	rts.push_back(conf[i]);

    _rtes.reserve(rts.size());

    if (cp_va_kparse(myconf, this, errh,
		     "BATCHER", cpkN, cpElementCast, "Batcher", &_batcher,
		     "TEST", cpkN, cpBool, &_test,
		     "NBITS", cpkN, cpInteger, &_lpm_bits,
		     "CPU", cpkN, cpInteger, &_on_cpu,
		     cpEnd) < 0)
	return -1;

    if (_on_cpu == 3) {
	errh->message("Skip mode.");
	return 0;
    }
    
    switch(_lpm_bits) {
    case 1:
    case 2:
    case 4:
	break;
    default:
	errh->error("LPM node bits error: %d", _lpm_bits);
	return -1;    
    }

    if (_on_cpu < 2)
	if (_batcher->req_anno(0, 4, BatchProducer::anno_write)) {
	    errh->error("Register annotation request in batcher failed");
	    return -1;
	}

    _psr.start = EthernetBatchProducer::ip4_hdr + 16; // 14
    _psr.start_offset = 0; // Dst IP Addr offset
    _psr.len = 4;
    _psr.end = _psr.start+_psr.start_offset+_psr.len;

    if (_on_cpu < 2)
	if (_batcher->req_slice_range(_psr) < 0) {
	    errh->error("Request slice range failed: %d, %d, %d, %d",
			_psr.start, _psr.start_offset, _psr.len, _psr.end);
	    return -1;
	} else
	    errh->message("Request slice range: %d, %d, %d, %d",
			_psr.start, _psr.start_offset, _psr.len, _psr.end);

    if (IPRouteTable::configure(rts, errh))
	return -1;

    return 0;
}

int
BIPLookup::add_route(const IPRoute& route, bool allow_replace,
		     IPRoute* replaced_route, ErrorHandler* errh)
{
    g4c_ipv4_rt_entry e;
    e.addr = ntohl(route.addr.addr());
    e.mask = ntohl(route.mask.addr());
    e.nnetbits = __builtin_popcount(e.mask);
    e.port = (uint8_t)route.port;

    _rtes.push_back(e);
    return 0;
}

int
BIPLookup::remove_route(const IPRoute& route, IPRoute* removed_route, ErrorHandler* errh)
{
    return -ENOENT;
}

int
BIPLookup::lookup_route(IPAddress addr, IPAddress& gw) const
{
    // Not called
    return 0;
}

String
BIPLookup::dump_routes()
{
    return String("BIPLookup");
}

int
BIPLookup::build_lpmt(vector<g4c_ipv4_rt_entry> &rtes, g4c_lpm_tree *&hlpmt,
		      g4c_lpm_tree *&dlpmt, int nbits, size_t &tsz, ErrorHandler *errh)
{
    g4c_ipv4_rt_entry *ents = new g4c_ipv4_rt_entry[rtes.size()];
    g4c_lpm_tree *t = 0;
    if (!ents) {
	errh->error("Out of memory for RT entries %lu", rtes.size());
        goto blerr_out;
    }

    memcpy(ents, rtes.data(), sizeof(g4c_ipv4_rt_entry)*rtes.size());
    
    t = g4c_build_lpm_tree(ents, rtes.size(), nbits, 0);
    if (!t) {
	errh->error("LPM tree building error");
	goto blerr_out;
    }

    tsz = sizeof(g4c_lpm_tree);
    switch(nbits) {
    case 1:
	tsz += sizeof(g4c_lpmnode1b_t)*t->nnodes;
	break;
    case 2:
	tsz += sizeof(g4c_lpmnode2b_t)*t->nnodes;
	break;
    case 4:
	tsz += sizeof(g4c_lpmnode4b_t)*t->nnodes;
	break;
    default:
	errh->error("FATAL: %d LPM node bits!", nbits);
	goto blerr_out;
    }

    hlpmt = (g4c_lpm_tree*)g4c_alloc_page_lock_mem(tsz);
    dlpmt = (g4c_lpm_tree*)g4c_alloc_dev_mem(tsz);
    if (hlpmt && dlpmt) {
	memcpy(hlpmt, t, tsz);
    } else {
	errh->error("Out of mem for lpmt, host %p, dev %p, size %lu.",
		    hlpmt, dlpmt, tsz);
	goto blerr_out;
    }

    free(t);
    delete[] ents;

    return 0;

blerr_out:
    if (ents)
	delete[] ents;
    if (t)
	free(t);

    if (hlpmt) {
	g4c_free_host_mem(hlpmt);
	hlpmt = 0;
    }

    if (dlpmt) {
	g4c_free_dev_mem(dlpmt);
	dlpmt = 0;
    }

    return -1;
}

int
BIPLookup::initialize(ErrorHandler *errh)
{
    if (_on_cpu == 3)
	return 0;
    
    if (build_lpmt(_rtes, _hlpmt, _dlpmt, _lpm_bits, _lpm_size, errh))
	return -1;

    int s = g4c_alloc_stream();
    if (!s) {
	errh->error("Failed to alloc stream for LPM copy");
	return -1;
    }

    g4c_h2d_async(_hlpmt, _dlpmt, _lpm_size, s);
    g4c_stream_sync(s);
    g4c_free_stream(s);

    errh->message("LPM tree built and copied to GPU.");

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
	    errh->message("BIPLookup anno offset %d", _anno_offset);

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
	} else {
	    errh->message("BIPLookup slice offset %d", _slice_offset);
	    for (int i=0; i<_batcher->nr_slice_ranges; i++) {
		errh->message("start %d, off %d, len %d, end %d",
			    _batcher->slice_ranges[i].start,
			    _batcher->slice_ranges[i].start_offset,
			    _batcher->slice_ranges[i].len,
			    _batcher->slice_ranges[i].end);
	    }
	}
    } else {
	_anno_offset = 0;
	_slice_offset = 30; // IP dst
    }
	
    return 0;
}

void
BIPLookup::bpush(int i, PBatch *p)
{
    if (!_on_cpu) {
	if (!p->dev_stream)
	    p->dev_stream = g4c_alloc_stream();
	g4c_ipv4_gpu_lookup_of(_dlpmt,
			       (uint32_t*)p->dslices(), _slice_offset,
			       p->producer->get_slice_stride(),
			       p->dannos(), _anno_offset, p->producer->get_anno_stride(),
			       _lpm_bits, p->npkts, p->dev_stream);
	p->hwork_ptr = p->hannos();
	p->dwork_ptr = p->dannos();
	p->work_size = p->npkts * p->producer->get_anno_stride();
    } else {
	if (_on_cpu == 3)
	    goto getout;
	
	if (_on_cpu < 2)
	    for(int j=0; j<p->npkts; j++) {
		*(int*)(g4c_ptr_add(p->anno_hptr(j), _anno_offset)) =
		    g4c_ipv4_lookup(_hlpmt, *(uint32_t*)(g4c_ptr_add(p->slice_hptr(j), _slice_offset)));
	    }
	else {
	    for (int j=0; j<p->npkts; j++) {
		Packet *pkt = p->pptrs[j];
		*(int*)(g4c_ptr_add(pkt->anno(), _anno_offset)) =
		    g4c_ipv4_lookup(_hlpmt, *(uint32_t*)(g4c_ptr_add(pkt->data(),
								     _slice_offset)));
	    }
	}
    }
getout:
    output(i).bpush(p);
}

void
BIPLookup::push(int i, Packet *p)
{
    if (_on_cpu < 2)
	hvp_chatter("Should never call this: %d, %p\n", i, p);
    else {
	if (_on_cpu != 3) {
	    *(int*)(g4c_ptr_add(p->anno(), _anno_offset)) =
		g4c_ipv4_lookup(_hlpmt, *(uint32_t*)(g4c_ptr_add(p->data(),
								 _slice_offset)));
	}
	output(i).push(p);
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRouteTable Batcher)
EXPORT_ELEMENT(BIPLookup)
ELEMENT_LIBS(-lg4c)
