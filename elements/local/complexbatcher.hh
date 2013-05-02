#ifndef CLICK_BATCHER_HH
#define CLICK_BATCHER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/pbatch.hh>
#include <g4c.h>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/sync.hh>
#include <click/ring.hh>
#include "belement.hh"
using namespace std;
CLICK_DECLS

#define CLICK_BATCH_TIMEOUT 2000

/**
 * ComplexBatcher: old.
 * Batcher configurations:
 *   TIMEOUT: int value in mili-sec.
 *   SLICE_BEGIN: int value
 *   SLICE_END: int value
 *   CAPACITY: int value for batch capacity
 *   ANN_BEGIN: unsigned char.
 *   ANN_END: unsigned char.
 *   FORCE_PKTLENS: bool value.
 *   BATCH_PREALLOC: int value.
 *   MT_PUSHERS: bool value.
 *   
 */
class ComplexBatcher : public BElement, public EthernetBatchProducer {
public:
    ComplexBatcher();
    ~ComplexBatcher();

    const char *class_name() const	{ return "ComplexBatcher"; }
    const char *port_count() const	{ return "1-/1"; }
    const char *processing() const  { return PUSH; }
    int configure_phase() const { return CONFIGURE_PHASE_LAST; }

    void push(int i, Packet *p);
    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

    void run_timer(Timer *timer);

private:
    int _batch_capacity;
    PBatch *_batch;
    int _slice_begin, _slice_end;
    unsigned char _anno_begin, _anno_end;
    bool _force_pktlens;
    bool _mt_pushers;
    int _test;

    int _timeout_ms;
    Timer _timer;
    PBatch *_timed_batch;

    int _count;
    int _drops;

    int _forced_nr_pools;
    bool _forced_alloc_locking;
    bool _forced_free_locking;

    bool _local_alloc;

public:
    virtual PBatch *alloc_batch();
    virtual int kill_batch(PBatch *pb);

protected:
    LFRing<PBatch*> *_pb_pools;
    volatile uint32_t *_pb_alloc_locks;
    volatile uint32_t *_pb_free_locks;
    volatile uint32_t _exp_pb_lock;
    int _nr_pools;
    bool _need_alloc_locking;
    bool _need_free_locking;
    int _nr_pre_alloc;

    int _batch_pool_size;

    // Call after configuration, need _mt_pushers, and other confs.
    virtual int init_pb_pool();

    // Really allocate a new PBatch.
    virtual PBatch *create_new_batch();

    // Initialize a batch after new creation.
    virtual int init_batch_after_create(PBatch* pb);

    // Reset args after allocting a batch from pool.
    virtual int init_batch_after_recycle(PBatch* pb) {
	pb->shared = 1;
	pb->tsed = false;
	return 0;
    }

    // Clean up batch for recycling it.
    virtual int finit_batch_for_recycle(PBatch *pb);

    // Try recycle batch, return false if not recycle-able.
    virtual bool recycle_batch(PBatch *pb);

    // Really destroy a batch.
    virtual int destroy_batch(PBatch *pb);

private:
    void add_packet(Packet *p);
};

CLICK_ENDDECLS
#endif
