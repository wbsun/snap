#ifndef CLICK_PBATCH_HH
#define CLICK_PBATCH_HH
#include <click/ipaddress.hh>
#include <click/glue.hh>
#include <g4c.h>

class Packet;

#define CLICK_PBATCH_PACKET_BUFFER_SIZE 2048
#define CLICK_PBATCH_CAPACITY 1024

CLICK_DECLS

class PBatch;

#ifndef CLICK_PBATCH_NR_RANGES
#define CLICK_PBATCH_NR_RANGES 8
#endif

struct PSliceRange {
#define pslice_special_range(psr) ((psr).start < 0)
#define PSLICE_FROM_ETH_HDR (-4096)
#define PSLICE_FROM_IP_HDR  (-4097)
#define PSLICE_FROM_IP_EXTENSION (-4098)
#define PSLICE_FROM_UDP_HDR (-4099)
#define PSLICE_FROM_TCP_HDR (-4100)
#define PSLICE_FROM_IP_PAYLOAD (-4101)
#define PSLICE_FROM_UDP_PAYLOAD (-4102)
#define PSLICE_FROM_TCP_PAYLOAD (-4103)
    int16_t start;
    int16_t start_offset;

#define pslice_special_length(psr) ((psr).len <= 0)
#define pslice_real_length(psr) ((psr).len < 0? -1*(psr).len:(psr).len)
#define PSLICE_LEN_UNKNOWN 0
    int16_t len;
    int16_t end;

    int16_t slice_offset;

    struct PSliceRange& operator=(const struct PSliceRange& v) {
	this->start = v.start;
	this->start_offset = v.start_offset;
	this->len = v.len;
	this->end = v.end;
	this->slice_offset = v.slice_offset;
	return *this;
    }
};

/*
 * Usage:
 *       Configuration phase: out of order
 *             set_batch_size();  // optional, self calls
 *             set_need_lens();   // optional, self or others call
 *             req_slice_raneg(); // optional, multiple times by others
 *             req_anno();        // optional, multiple times by others
 *             req_priv_data();   // optional, multiple times by others
 *
 *       After other's configuration, in order by self
 *             setup_anno();
 *             setup_slice_ranges();
 *             setup_mm();
 *             or simply setup_all().
 *
 *             Then initialize PBatch pool if exists.
 *
 * For PBatch new creation:
 *       pb = new PBatch(self);  
 *       pb->init();
 *       alloc_batch_priv_data(pb);
 *       assign_batch_mem(pb, ... );
 *
 * For PBatch alloc:
 *       pb = alloc_batch();
 *
 * For PBatch recyling:
 *       kill_batch(pb);
 */
class BatchProducer {
public:
    bool setup_done;
    BatchProducer() {
	setup_done = false;
	this->init_slice_ranges();
	this->init_anno();
	this->init_mm();
#ifndef CLICK_NO_BATCH_TEST    
	test_mode = 0;
#endif
    }
    
    enum {test_mode1 = 2, test_mode2=3, test_mode3=4};
#ifndef CLICK_NO_BATCH_TEST    
    int test_mode;
#endif

    //
    // Packet slice settings:
    //
public:    
    int nr_slice_ranges_req;
    struct PSliceRange req_slice_ranges[CLICK_PBATCH_NR_RANGES];
    int nr_slice_ranges;
    struct PSliceRange slice_ranges[CLICK_PBATCH_NR_RANGES];

    int16_t slice_length; // = -1
    int16_t slice_stride; // = -1

    virtual void calculate_slice_args();

public:
    int get_nr_slice_ranges();
    virtual void init_slice_ranges();
    
    // Return idx of allocated slice range in req_slice_ranges,
    //  or -1 if full.
    virtual int req_slice_range(PSliceRange &psr);

    // Could be no solution, and whole packet as a result.
    virtual void setup_slice_ranges() = 0;

    // Return offset of request slice in slice buffers, or -1 if not exist.
    virtual int16_t get_slice_offset(const PSliceRange &psr) = 0;

    virtual int16_t get_slice_length();
    virtual int16_t get_slice_stride();

    
    //
    // Annotations:
    //
public:
    uint8_t w_anno_start;
    uint8_t w_anno_len;
    uint8_t r_anno_start;
    uint8_t r_anno_len;

    uint8_t anno_start;
    uint8_t anno_len;

public:
    enum {anno_write = 0x1, anno_read = 0x2, anno_ondev = 0x4};

    virtual void init_anno();    
    virtual int req_anno(uint8_t start, uint8_t len, uint8_t rw);
    virtual void setup_anno();
    virtual int8_t get_anno_offset(uint8_t start);
    virtual uint8_t get_anno_stride() {
	return anno_len;
    }
    
    //
    // Memory management:
    //
public:
    int batch_size;
    size_t mem_size;
    int lens_offset;
    int slices_offset;
    int annos_offset;
    bool need_lens;
    
public:
    void set_batch_size(int bsz) { batch_size = bsz; }
    void set_need_lens() { need_lens = true; }
    virtual int assign_batch_mem(PBatch *pb, void* hm,
				 void *dm, size_t msz);
    virtual void init_mm();
    virtual void setup_mm();

    inline bool has_lens() {
	return lens_offset >= 0; }
    inline bool has_slices() {
	return slices_offset >= 0; }
    inline bool has_annos() {
	return annos_offset >= 0; }


    //
    // Private data for batch users
    //
public:
    int nr_batch_users;
    size_t batch_priv_len;

public:
    // Return offset of request data in private buffer.
    virtual size_t req_priv_data(size_t len);
    virtual void init_priv_data();

    // Return negative value on error 
    virtual int alloc_batch_priv_data(PBatch *pb);
    
public:
    // Call after init_xxx, req_xxx, set_xxx:
    virtual void setup_all() {
	if (setup_done)
	    return;
	
	this->setup_anno();
	this->setup_slice_ranges();
	this->setup_mm();
	this->init_priv_data();
	setup_done = true;
    }

    //
    // PBatch (pool) management:
    //
public:
    // Allocate a new PBatch, may from pool (optional).
    virtual PBatch* alloc_batch() = 0;

    // Try to destroy batch:
    //        if pb shared by others, just dec shared, rt 1.
    //        if not shared, try recycle. (optional). rt 0.
    //        if not recycle-able, destroy it. rt 2.
    //        on error, return negative.
    virtual int kill_batch(PBatch *pb) = 0;

    friend class PBatch;
};

class EthernetBatchProducer : public BatchProducer {
public:
    EthernetBatchProducer() : BatchProducer() {}
    
    static const int16_t eth_hdr;
    static const int16_t ip4_hdr;
    static const int16_t ip4_ext;
    static const int16_t arp_hdr;
    static const int16_t udp4_hdr;
    static const int16_t tcp4_hdr;
    static const int16_t eth_payload;
    static const int16_t ip4_payload;
    static const int16_t udp4_payload;
    static const int16_t tcp4_payload;
    static const int16_t ip6_hdr;
    static const int16_t ip6_ext;
    static const int16_t ip6_payload;
    static const int16_t udp6_hdr;
    static const int16_t tcp6_hdr;
    static const int16_t udp6_payload;
    static const int16_t tcp6_payload;

    // To limit wasted bandwidth becasue of range merging.
    //    factor: useless_len / (useless_len + effective_len)
    static float merge_threshold;
    
    virtual void setup_slice_ranges();
    virtual int16_t get_slice_offset(const PSliceRange &psr);

    virtual PBatch* alloc_batch() = 0;
    virtual int kill_batch(PBatch *pb) = 0;

    friend class PBatch;
};

class PBatch {
public:
    BatchProducer *producer;
    int npkts;
    Packet **pptrs;

    void *host_mem;
    void *dev_mem;

    int dev_stream;
    void *hwork_ptr;
    void *dwork_ptr;
    int work_size;

    volatile int shared;

    void *priv_data;
    bool tsed;
    timespec ts;

    PBatch *list_node1;
    PBatch *list_node2;

public:
    PBatch(BatchProducer *prod);
    virtual ~PBatch();

    int init();
    void finit();

    void kill() {
	producer->kill_batch(this);
    }

public:
    inline uint8_t* hslices() {
	if (producer->slices_offset < 0)
	    return 0;
	return (uint8_t*)g4c_ptr_add(
	    host_mem,
	    producer->slices_offset);
    }

    inline uint8_t* dslices() {
	if (producer->slices_offset < 0)
	    return 0;
	return (uint8_t*)g4c_ptr_add(
	    dev_mem,
	    producer->slices_offset);
    }

    inline uint8_t* hannos() {
	if (producer->annos_offset < 0)
	    return 0;
	return (uint8_t*)g4c_ptr_add(
	    host_mem,
	    producer->annos_offset);
    }

    inline uint8_t* dannos() {
	if (producer->annos_offset < 0)
	    return 0;
	return (uint8_t*)g4c_ptr_add(
	    dev_mem,
	    producer->annos_offset);
    }

    inline int16_t* hlens() {
	if (producer->lens_offset < 0)
	    return 0;
	return (int16_t*)g4c_ptr_add(
	    host_mem,
	    producer->lens_offset);
    }

    inline int16_t* dlens() {
	if (producer->lens_offset < 0)
	    return 0;
	return (int16_t*)g4c_ptr_add(
	    dev_mem,
	    producer->lens_offset);
    }
    
    // Accessors:
    inline uint8_t* slice_hptr(int idx) {
	if (producer->slices_offset < 0)
	    return 0;
	return (uint8_t*)g4c_ptr_add(
	    host_mem,
	    producer->slices_offset+producer->get_slice_stride()*idx);
    }

    inline int16_t* length_hptr(int idx) {
	if (producer->lens_offset < 0)
	    return 0;
	return (int16_t*)g4c_ptr_add(
	    host_mem,
	    producer->lens_offset+sizeof(int16_t)*idx);
    }

    inline uint8_t* anno_hptr(int idx) {
	if (producer->annos_offset < 0)
	    return 0;
	return (uint8_t*)g4c_ptr_add(
	    host_mem,
	    producer->annos_offset+producer->anno_len*idx);
    }

    inline void* get_priv_data(size_t offset) {
	if (priv_data)
	    return g4c_ptr_add(priv_data, offset);
	return 0;
    }
};

CLICK_ENDDECLS
#endif
