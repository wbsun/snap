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

    // click_chatter("Request %p slice range %d, start %d, ofs %d, "
		  // "len %d, end %d\n", this,
		  // nr_slice_ranges_req,
		  // psr.start, psr.start_offset, psr.len, psr.end);
    
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
    click_chatter("Request %lu private data from batcher %p, total %lu\n",
		  len, this, batch_priv_len);

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
	    click_chatter("PBatch out of mem for private data %lu.\n",
		batch_priv_len);
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

float EthernetBatchProducer::merge_threshold = 0.6f;


bool
__comp_slice_range(pair<int16_t, int16_t> a, pair<int16_t, int16_t> b)
{
    return a.first < b.first;
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
	slice_ranges[0].end = slice_ranges[0].start + slice_ranges[0].len;
	slice_ranges[0].slice_offset = 0;
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

    // do {
	// list<pair<int16_t, int16_t> >::iterator lite = pl1->begin();
	// click_chatter("sorted ranges:");
	// while (lite != pl1->end()) {
	    // click_chatter("(%d, %d)", lite->first, lite->second);
	    // ++lite;
	// }
    // } while (0);

    // First merge, which merges overlapped ranges only:
    int16_t tlen = 0;
    list<pair<int16_t, int16_t> >::iterator ite = pl1->begin();
    pair<int16_t, int16_t> prev = *ite;
    ++ite;
    while(ite != pl1->end())
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
	++ite;
    }
    pl2->push_back(prev);
    tlen += prev.second - prev.first;

    pl1->clear();
    tmp = pl1;
    pl1 = pl2;
    pl2 = tmp;

    // do {
	// list<pair<int16_t, int16_t> >::iterator lite = pl1->begin();
	// click_chatter("first merged ranges:");
	// while (lite != pl1->end()) {
	    // click_chatter("(%d, %d)", lite->first, lite->second);
	    // ++lite;
	// }
    // } while (0);


    // Then merge non-overlapped ranges, gaps between which are
    //   larger than merge_threshold * tlen.
    if (pl1->size() > 1)
    {
	bool merged;
	do {
	    merged = false;
	    
	    ite = pl1->begin();
	    prev = *ite;
	    if (ite != pl1->end()) {
		++ite;
		while (ite != pl1->end()) {
		    pair<int16_t, int16_t> cur = *ite;

		    if ((float)(((float)(cur.first - prev.second))/
				((float)(tlen)))
			<= EthernetBatchProducer::merge_threshold) {
			tlen += cur.first-prev.second;
			
			prev.second = cur.second;
			merged = true;
		    } else {
			pl2->push_back(prev);
			prev = cur;
		    }
		    ++ite;
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

	// click_chatter("Final slice of %p: %d, start %d, ofs %d, len %d "
		      // "end %d, slice_ofs %d\n", this, nr_slice_ranges,
		      // slice_ranges[nr_slice_ranges].start,
		      // slice_ranges[nr_slice_ranges].start_offset,
		      // slice_ranges[nr_slice_ranges].len,
		      // slice_ranges[nr_slice_ranges].end,
		      // slice_ranges[nr_slice_ranges].slice_offset);

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
    tsed = false;
    npkts = 0;
    host_mem = 0;
    dev_mem = 0;
    dev_stream = 0;
    hwork_ptr = 0;
    dwork_ptr = 0;
    work_size = 0;
    shared = 0;
    priv_data = 0;

    list_node1 = 0;
    list_node2 = 0;
    
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

CLICK_ENDDECLS
