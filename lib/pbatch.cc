#include <click/config.h>
#include <click/glue.hh>
#include <click/pbatch.hh>
#include <g4c.h>

#include <utility>
#include <list>
using namespace std;

CLICK_DECLS

int
BatchProducer::get_nr_slice_ranges()
{
    return nr_slice_ranges;
}

void
BatchProducer::init_slice_ranges()
{
    nr_slice_ranges_req = 0;
    nr_slice_ranges = 0;
    slice_length = -1;
    slice_stride = -1;
}

void
BatchProducer::calculate_slice_args()
{
    slice_length = 0;
    slice_stride = 0;

    for (int i=0; i<nr_slice_ranges; i++)
	slice_length += pslice_real_length(slice_ranges[i]);

    if (slice_length)
	slice_stride = g4c_round_up(slice_length, G4C_MEM_ALIGN);
}

int16_t
BatchProducer::get_slice_length()
{
    if (slice_length < 0)
	this->calculate_slice_args();

    return slice_length;
}

int16_t
BatchProducer::get_slice_stride()
{
    if (slice_stride < 0)
	this->calculate_slice_args();

    return slice_stride;
}

int
BatchProducer::req_slice_range(PSliceRange &psr)
{
    if (nr_slice_ranges_req >= CLICK_PBATCH_NR_RANGES)
	return -1;
    
    req_slice_ranges[nr_slice_ranges_req] = psr;
    return nr_slice_ranges_req++;
}


#define __min(a,b) ({						\
	    typeof(a) _a = a;					\
	    typeof(b) _b = b;					\
	    _a < _b? _a:_b; })
#define __max(a,b) ({						\
	    typeof(a) _a = a;					\
	    typeof(b) _b = b;					\
	    _a > _b? _a:_b; })
#define __merge_anno_range(sdst, ldst, snew, lnew) 		\
	if ((ldst) == 0) {					\
	    ldst = (lnew);					\
	    sdst = (snew);					\
	} else {						\
	    typeof(sdst) ns = 0, ne = 0;			\
	    ns = __min(sdst, snew);				\
	    ne = __max((sdst)+(ldst), (snew)+(lnew));		\
	    sdst = ns;						\
	    ldst = ne - ns; }

    
void
BatchProducer::init_anno()
{
    w_anno_start = 0;
    w_anno_len = 0;
    r_anno_start = 0;
    r_anno_len = 0;
    anno_start = 0;
    anno_len = 0;
}

int
BatchProducer::req_anno(uint8_t start, uint8_t len, uint8_t rw)
{
    if (rw & anno_write) {	    
	__merge_anno_range(w_anno_start, w_anno_len,
			   start, len);
    }
    
    if (rw & anno_read) {
	__merge_anno_range(r_anno_start, r_anno_len,
			   start, len);
    }
    return 0;
}

void
BatchProducer::setup_anno()
{
    anno_len = w_anno_len;
    anno_start = w_anno_start;
    if (r_anno_len != 0) {
	__merge_anno_range(anno_start, anno_len,
			   r_anno_start, r_anno_len);
    }
}

int8_t
BatchProducer::get_anno_offset(uint8_t start)
{
    if (start >= anno_start)
	return start - anno_start;
    return -1;
}

int
BatchProducer::assign_batch_mem(PBatch *pb, void *hm, void *dm, size_t msz)
{
    pb->host_mem = hm;
    pb->dev_mem = dm;
    return 0;
}

void
BatchProducer::init_mm()
{
    mem_size = 0;
    lens_offset = -1;
    slices_offset = -1;
    annos_offset = -1;
    need_lens = false;
    batch_size = CLICK_PBATCH_CAPACITY;
}

void
BatchProducer::setup_mm()
{
    if (need_lens) {
	mem_size += g4c_round_up(batch_size*sizeof(int16_t), G4C_PAGE_SIZE);
	lens_offset = 0;
    }
    
    if (this->get_slice_stride() && batch_size) {
	slices_offset = mem_size;
    }
    mem_size += g4c_round_up(batch_size*this->get_slice_stride(), G4C_PAGE_SIZE);

    if (anno_len != 0) {
	annos_offset = mem_size;
	mem_size += g4c_round_up(batch_size*anno_len, G4C_PAGE_SIZE);
    }    
}

void
BatchProducer::init_priv_data()
{
    nr_batch_users = 0;
    batch_priv_len = 0;
}

size_t
BatchProducer::req_priv_data(size_t len)
{
    nr_batch_users++;
    size_t pos = batch_priv_len;
    batch_priv_len += len;

    return pos;
}

int
BatchProducer::alloc_batch_priv_data(PBatch *pb)
{
    if (pb->priv_data)
	free(pb->priv_data);

    pb->priv_data = 0;
    if (batch_priv_len) {
	pb->priv_data = malloc(batch_priv_len);
	if (!pb->priv_data) {
	    click_chatter("PBatch out of mem for private data.\n");
	    return -1;
	}
    }

    return 0;
}


// EthernetBatchProducer

const int16_t EthernetBatchProducer::eth_hdr      = 0;
const int16_t EthernetBatchProducer::ip4_hdr      = 14;
const int16_t EthernetBatchProducer::ip4_ext      = 34;
const int16_t EthernetBatchProducer::arp_hdr      = 14;
const int16_t EthernetBatchProducer::udp4_hdr     = 34;
const int16_t EthernetBatchProducer::tcp4_hdr     = 34;
const int16_t EthernetBatchProducer::eth_payload  = 14;
const int16_t EthernetBatchProducer::ip4_payload  = 34;
const int16_t EthernetBatchProducer::udp4_payload = 42;
const int16_t EthernetBatchProducer::tcp4_payload = 54;
const int16_t EthernetBatchProducer::ip6_hdr      = 14;
const int16_t EthernetBatchProducer::ip6_ext      = 54;
const int16_t EthernetBatchProducer::ip6_payload  = 54;
const int16_t EthernetBatchProducer::udp6_hdr     = 54;
const int16_t EthernetBatchProducer::tcp6_hdr     = 54;
const int16_t EthernetBatchProducer::udp6_payload = 62;
const int16_t EthernetBatchProducer::tcp6_payload = 74;

float EthernetBatchProducer::merge_threshold = 0.2f;


static bool
__comp_slice_range(pair<int16_t, int16_t> a, pair<int16_t, int16_t> b)
{
    return a.first <= b.first;
}

void
EthernetBatchProducer::setup_slice_ranges()
{
    // Rare case:
    if (nr_slice_ranges_req == 0) {
	nr_slice_ranges = 0;
	return;
    }
    
    // Special but may be common case:
    if (nr_slice_ranges_req == 1)
    {
	nr_slice_ranges = 1;

	slice_ranges[0].start = req_slice_ranges[0].start+
	    req_slice_ranges[0].start_offset;
	slice_ranges[0].start_offset = 0;
	slice_ranges[0].len = pslice_real_length(req_slice_ranges[0]);
	return;
    }

    
    // Then convert ranges to pairs:
    list<pair<int16_t, int16_t> > l1, l2, *pl1, *pl2, *tmp;

    pl1 = &l1;
    pl2 = &l2;
    for (int i = 0; i<nr_slice_ranges_req; i++) {
	int16_t ns = req_slice_ranges[i].start+
	    req_slice_ranges[i].start_offset;
	int16_t ne = ns+pslice_real_length(req_slice_ranges[i]);

	pl1->push_back(make_pair(ns, ne));
    }

    // Sort so that go from the beginning:
    pl1->sort(__comp_slice_range);

    // First merge, which merges overlapped ranges only:
    int16_t tlen = 0;
    list<pair<int16_t, int16_t> >::iterator ite = pl1->begin();
    pair<int16_t, int16_t> prev = *ite;
    while(ite++ != pl1->end())
    {
	pair<int16_t, int16_t> cur = *ite;

	if (cur.first <= prev.second)
	{
	    pair<int16_t, int16_t> t;
	    
	    t.first = prev.first;
	    t.second = __max(prev.second, cur.second);
	    prev = t;
	}
	else
	{
	    pl2->push_back(prev);
	    tlen += prev.second - prev.first;
	    prev = cur;
	}
    }
    pl2->push_back(prev);
    tlen += prev.second - prev.first;

    pl1->clear();
    tmp = pl1;
    pl1 = pl2;
    pl2 = tmp;

    // Then merge non-overlapped ranges, gaps between which are
    //   larger than merge_threshold * tlen.
    if (pl1->size() > 1)
    {
	bool merged;
	do {
	    merged = false;
	    
	    ite = pl1->begin();
	    prev = *ite;
	    while (ite++ != pl1->end()) {
		pair<int16_t, int16_t> cur = *ite;

		if ((float)(((float)(cur.first - prev.second))/
			    ((float)(tlen)))
		    >= EthernetBatchProducer::merge_threshold) {
		    tlen += cur.first-prev.second;

		    prev.second = cur.second;
		    merged = true;
		} else {
		    pl2->push_back(prev);
		    prev = cur;
		}		
	    }
	    pl2->push_back(prev);

	    pl1->clear();
	    tmp = pl1;
	    pl1 = pl2;
	    pl2 = tmp;
	} while (merged && pl1->size() > 1);
    }

    // Set final results:
    ite = pl1->begin();
    nr_slice_ranges = 0;
    int16_t offset = 0;
    while(ite != pl1->end()) {
	slice_ranges[nr_slice_ranges].start = ite->first;
	slice_ranges[nr_slice_ranges].start_offset = 0;
	slice_ranges[nr_slice_ranges].len = ite->second - ite->first;
	slice_ranges[nr_slice_ranges].end = ite->second;
	slice_ranges[nr_slice_ranges].slice_offset = offset;

	nr_slice_ranges++;
	offset += ite->second - ite->first;
	++ite;
    }
}

int16_t
EthernetBatchProducer::get_slice_offset(const PSliceRange &psr)
{
    // negative is ignored
    int16_t start = psr.start + psr.start_offset;

    for (int i=0; i<nr_slice_ranges; i++) {
	if (slice_ranges[i].start <= start &&
	    slice_ranges[i].end >= psr.end)
	    return slice_ranges[i].slice_offset;

	// Should be, but in fact does not need to be.
// 	if (slice_ranges[i].start <= start &&
// 	    slice_ranges[i].end >= start + pslice_real_length(psr))
// 	    return slice_ranges[i].slice_offset;
    }

    return -1;
}


PBatch::PBatch(BatchProducer *prod) : producer(prod)
{
}

PBatch::~PBatch()
{
    finit();
}

int
PBatch::init()
{
    npkts = 0;
    host_mem = 0;
    dev_mem = 0;
    dev_stream = 0;
    hwork_ptr = 0;
    dwork_ptr = 0;
    work_size = 0;
    shared = 1;
    priv_data = 0;
    
    pptrs = new Packet*[producer->batch_size];
    if (!pptrs) {
	click_chatter("PBatch out of mem for Packet pointers.");
	return -1;
    }
    return 0;
}

void
PBatch::finit()
{
    if (pptrs)
	delete[] pptrs;
    if (priv_data)
	free(priv_data);
}


#if 0

PBatch::PBatch(): capacity(CLICK_PBATCH_CAPACITY), npkts(0), pptrs(0), memsize(0),
		  hostmem(0), devmem(0), hpktlens(0), hslices(0), hpktannos(0),
		  dpktlens(0), dslices(0), dpktannos(0), slice_begin(0),
		  slice_end(0), slice_length(0), slice_size(0),
		  anno_flags(0), anno_size(0), dev_stream(0),
		  hwork_ptr(0), dwork_ptr(0), work_size(0),
		  force_pktlens(false), shared(0), nr_users(0), user_priv_len(0),
		  user_priv(0), hpktflags(0), dpktflags(0)
{
}

PBatch::PBatch(int _capacity, int _slice_begin, int _slice_end, bool _force_pktlens,
	       int _anno_flags, int _anno_length):
    capacity(_capacity), npkts(0), pptrs(0),
    hostmem(0), devmem(0), hpktlens(0), hslices(0), hpktannos(0),
    dpktlens(0), dslices(0), dpktannos(0),
    anno_flags(_anno_flags), dev_stream(0),
    hwork_ptr(0), dwork_ptr(0), work_size(0), force_pktlens(_force_pktlens),
    shared(0), nr_users(0), user_priv_len(0), user_priv(0),
    hpktflags(0), dpktflags(0)
{
    slice_begin = _slice_begin;
    slice_end = _slice_end;
    anno_size = _anno_length;
    calculate_parameters();
}


PBatch::~PBatch() {}

void
PBatch::calculate_parameters()
{
    if (slice_end < 0) {
	slice_length = CLICK_PBATCH_PACKET_BUFFER_SIZE;
	slice_size = slice_length;
    } else {
	slice_length = slice_end - slice_begin;
	slice_size = g4c_round_up(slice_length, G4C_MEM_ALIGN);
    }

    if (anno_flags == 0)
	anno_size = 0;
    else
	anno_size = g4c_round_up(anno_size, G4C_MEM_ALIGN);

    memsize = 0;
    if (slice_end < 0||force_pktlens)
	memsize += g4c_round_up(sizeof(short)*capacity, G4C_PAGE_SIZE);

    memsize += g4c_round_up(sizeof(unsigned int)*capacity, G4C_PAGE_SIZE);
    memsize += g4c_round_up(slice_size*capacity, G4C_PAGE_SIZE);

    if (anno_flags != 0)
	memsize += g4c_round_up(anno_size*capacity, G4C_PAGE_SIZE);
}

/**
 * Set pointers to regions of memory chunk.
 * Pre-condition: hostmem and devmem must be set before calling this.
 */
void
PBatch::set_pointers()
{
    if (slice_end >= 0 && !force_pktlens) {
	hpktlens = 0;
	dpktlens = 0;

	hpktflags = (unsigned int*)hostmem;
	dpktflags = (unsigned int*)devmem;
    } else {
	hpktlens = (short*)hostmem;
	dpktlens = (short*)devmem;

	hpktflags = (unsigned int*)g4c_ptr_add(
	    hpktlens,
	    g4c_round_up(sizeof(short)*capacity, G4C_PAGE_SIZE));
	dpktflags = (unsigned int*)g4c_ptr_add(
	    dpktlens,
	    g4c_round_up(sizeof(short)*capacity, G4C_PAGE_SIZE));
    }

    hslices = (unsigned char*)g4c_ptr_add(
	hpktflags,
	g4c_round_up(sizeof(unsigned int)*capacity, G4C_PAGE_SIZE));
    dslices = (unsigned char*)g4c_ptr_add(
	dpktflags,
	g4c_round_up(sizeof(unsigned int)*capacity, G4C_PAGE_SIZE));					      

    if (anno_flags == 0) {
	hpktannos = 0;
	dpktannos = 0;
    } else {
	hpktannos = hslices +
	    g4c_round_up(slice_size*capacity, G4C_PAGE_SIZE);
	dpktannos = dslices +
	    g4c_round_up(slice_size*capacity, G4C_PAGE_SIZE);
    }
}

int
PBatch::init_for_host_batching()
{
    if (memsize == 0) // unstable sign
	calculate_parameters();
	
    pptrs = new Packet*[capacity];
    if (!pptrs)
	return -1;
	
    memset(pptrs, 0, sizeof(Packet*)*capacity);
    return 0;
}

void
PBatch::clean_for_host_batching()
{
    delete[] pptrs;
}

#endif

CLICK_ENDDECLS
