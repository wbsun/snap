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
	this->slice_offset = v.offset;
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
 *             or simply setup().
 *
 *             Then initialize PBatch pool if exists.
 *
 * For PBatch creation:
 *       pb = new PBatch(self);  
 *       pb->init();
 *       alloc_batch_priv_data(pb);
 *       assign_batch_mem(pb, ... );
 *
 * For PBatch recyling:
 *       pb->recyle();
 */
class BatchProducer {
public:
    BatchProducer() {
	this->init_slice_ranges();
	this->init_anno();
	this->init_mm();
    }


    //
    // Packet slice settings:
    //
protected:    
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
    enum {anno_write = 0x1, anno_read = 0x2};

    virtual void init_anno();    
    virtual int req_anno(uint8_t start, uint8_t len, uint8_t rw);
    virtual void setup_anno();
    virtual int8_t get_anno_offset(uint8_t start);

    
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
    virtual void setup() {
	this->setup_anno();
	this->setup_slice_ranges();
	this->setup_mm();
    }

    // Return 0 for OK recycled
    //        1 for not recycle-able
    //        negative for errors
    virtual int recycle_batch(PBatch *pb) = 0;

    // Try to destroy batch:
    //        if pb shared by others, just dec shared.
    //        if not shared, try recycle.
    //        if not recycle-able, destroy it.
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

    virtual int recycle_batch(PBatch *pb) = 0;
    virtual int kill_batch(PBatch *pb) = 0;

    firend class PBatch;
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

    int shared;

    void *priv_data;

public:
    PBatch(BatchProducer *prod);
    virtual ~PBatch();

    void init();
    void finit();
};

#if 0  // Temporarily keep this because of lagecy uses of PBatch.
 
class PBatch {
public:
    int capacity;
    int npkts;
    Packet **pptrs;

    unsigned long memsize;	
    void *hostmem;
    void *devmem;

    /*
     * Packet memory layout on host and on device:
     *
     *  +----------------------------------+
     *  |                                  |
     *  |  N * short: packet lengths       |
     *  |                                  |
     *  |----------------------------------|
     *  ~  For roundup to PAGE_SIZE        ~
     *  |----------------------------------|
     *  |                                  |
     *  |  N * unsigned int: packet flags  |
     *  |                                  | 
     *  |----------------------------------|
     *  ~  For roundup to PAGE_SIZE        ~
     *  |----------------------------------|
     *  |                                  |
     *  |  N * slice_size: packet data     |
     *  |                                  |
     *  |----------------------------------|
     *  ~  For roundup to PAGE_SIZE        ~
     *  |----------------------------------|
     *  |                                  |
     *  |  N * anno_size: annotation data  |
     *  |                                  |
     *  +----------------------------------+
     *
     *  Packet lengths data may not be copied if the slice length
     *  doesn't include negative value, which means max slice.
     *  In fact, no packet length data mem allocated at all if
     *  slice range are all positive.
     *
     *  Annotation data may also not be copied if anno_flags is
     *  0 or for write only. Particularly, if 0, no annotation data
     *  are allocated at all.
     */
	
    /*
     * Packet lengths for each packet.
     * When negative, the packet is disabled.
     */
    short *hpktlens;
    unsigned int *hpktflags; /* 0 means normal, availabe.  */
    unsigned char *hslices;
    unsigned char *hpktannos;

    // Device pointers
    short *dpktlens;
    unsigned int *dpktflags;
    unsigned char *dslices;
    unsigned char *dpktannos;

    /*
     * Slice is a piece of packet data. A slice is copied from Packet's data to
     * page-locked host memory, and copied to device. Each packet has a slice.
     * Slice is only part of the packet data. The range is assigned by the batcher
     * that creates the PBatch. Batcher knows slice range via device elements
     * that use the batcher. Batcher element is assigned to device elements in
     * configuration file. During configuring, device elements call Batcher's
     * set_slice_range() to set the range they need. The final range may be
     * larger than the one a device element specified because a Batcher could
     * be shared by multiple device elements that need different ranges.
     * But set_slice_range() will find their union without gap.
     *
     * Vars:
     *   Slice range is [slice_begin, slice_end). Close beginning and open ending.
     *   slice_begin:  begin position in packet data to form slice.
     *   slice_end:    end position(excluded) in packet data to form slice, a negative
     *                 slice_end means copy the entire packet data.
     *   slice_length: the finally allocated slice length, usually
     *                 it is slice_end - slice_begin.
     *   slice_size:   the memory allocated to hold a slice. For round up to
     *                 DW or QW or other alignments.
     *   anno_flags:   flags to indicate whether annotation needed, to produce
     *                 or to use or both.
     *   anno_size:    the memory allcoated to hold a packet annotation, for
     *                 roundup. Must be greater than or equal to Packet::anno_size.
     *
     */
    int slice_begin;
    int slice_end;
    int slice_length; // finally allocated slice length
    int slice_size; // memory size allocated to hold a slice.
    bool force_pktlens;
    unsigned char anno_flags;
#define PBATCH_ANNO_READ ((unsigned char)0x01)
#define PBATCH_ANNO_WRITE ((unsigned char)0x02)
    int anno_size; 

    /*
     * For D2H and H2D copy elements, and device execution elements.
     *   dev_stream: stream id used for device operations.
     *   hwork_ptr: host side work pointer, for current copy and execution data.
     *   dwork_ptr: device side work pointer.
     *   work_size: current work data size.
     */
    int dev_stream;
    void *hwork_ptr;
    void *dwork_ptr;
    int work_size;

    // for sharing:
    int shared;
    int nr_users;
    unsigned long user_priv_len;
    void *user_priv;	

public:
    // Functions:
    PBatch();
    PBatch(int _capacity, int _slice_begin, int _slice_end, bool _force_pktlens,
	   int _anno_flags, int _anno_length);
    ~PBatch();
    void calculate_parameters();
    int init_for_host_batching();
    void clean_for_host_batching();
    void set_pointers();
    inline bool full() { return npkts >= capacity;}
    inline int size() { return npkts; }

    inline unsigned int *hpktflag(int idx) { return hpktflags + idx; }
    inline unsigned char *hslice(int idx) { return hslices + idx*slice_size;}
    inline unsigned char *hanno(int idx) { return hpktannos?(hpktannos + idx*anno_size):0;}
    inline short *hpktlen(int idx) { return hpktlens?(hpktlens + idx):0;}

    inline void *get_user_priv(unsigned long offset) { return (void*)(g4c_ptr_add(user_priv, offset)); }
};

#endif

CLICK_ENDDECLS
#endif
