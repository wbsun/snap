#ifndef CLICK_ST_BATCHER_HH
#define CLICK_ST_BATCHER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/pbatch.hh>
#include <g4c.h>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/sync.hh>
#include <pthread.h>
#include "belement.hh"

CLICK_DECLS

#define CLICK_BATCH_TIMEOUT 2000

/**
 * STBatcher memory pool model:
 *   Two kinds of lists:
 *       Allocation list: Used to allocate new batch, only one
 *                        list per batcher, for single thread
 *                        allocator only.
 *       Free list: Used to hold batches freed by other elements,
 *                  one list per thread, thread access local list
 *                  only.
 *   One list of free lists: Used to hold free lists. Added by free
 *                           removed by allocation.
 *
 *   Mechanism:
 *             Single thread allocator, multithread free.
 */
class Batcher : public BElement, EthernetBatchProducer {
public:
    Batcher() {
	arg_pre_alloc_size = 1024;
	batch_size = CLICK_PBATCH_CAPACITY;
	arg_max_free_list_size = 16;
	arg_work_mode = 0;
	
	_batch = 0;
	arg_timeout_ms = 0;
	_timed_batch = 0;
	_count = 0;
	_drops = 0;
    }
    ~Batcher();

    const char *class_name() const	{ return "Batcher"; }
    const char *port_count() const	{ return "1-/1"; }
    const char *processing() const  { return PUSH; }
    int configure_phase() const { return CONFIGURE_PHASE_LAST; }

    void push(int i, Packet *p);
    int configure(Vector<String> &conf, ErrorHandler *errh) {
	int arg_slice_begin = 0,
	    arg_slice_end = 0,
	    arg_anno_begin = 0,
	    arg_anno_end = 0;
	    
	if (cp_va_kparse(
		conf, this, errh,
		"TIMEOUT", cpkN, cpInteger, &arg_timeout_ms,
		"SLICE_BEGIN", cpkN, cpInteger, &arg_slice_begin,
		"SLICE_END", cpkN, cpInteger, &arg_slice_end,
		"CAPACITY", cpkN, cpInteger, &batch_size,
		"ANN_BEGIN", cpkN, cpByte, &arg_anno_begin,
		"ANN_END", cpkN, cpByte, &arg_anno_end,
		"FORCE_PKTLENS", cpkN, cpBool, &need_lens,
		"BATCH_PREALLOC", cpkN, cpInteger, &arg_pre_alloc_size,
		"FREE_LIST_SIZE", cpkN, cpInteger,
		&arg_max_free_list_size,
		"MODE", cpkN, cpInteger, &arg_work_mode,
		cpEnd) < 0)
	    return -1;

	if(arg_slice_end) {
	    PSliceRange psr;
	    psr.start = arg_slice_begin;
	    psr.start_offset = 0;
	    psr.len = arg_slice_end - arg_slice_begin;
	    psr.end = arg_slice_end;

	    req_slice_range(psr);
	}

	if (arg_anno_end != 0) {
	    req_anno(arg_anno_begin, arg_anno_end-arg_anno_begin,
		     anno_write);
	}

	setup_all();
	return 0;	
    }
    
    int initialize(ErrorHandler *errh) {
	_timer.initialize(this);

	init_pb_pool();
	return 0;
    }

    void run_timer(Timer *timer);

    virtual void* cast(const char* name) {
	if (strcmp(name, "Batcher") == 0)
	    return (Batcher*)this;
	else if (strcmp(name, "BElement") == 0)
	    return (BElement*)this;
	else
	    return BElement::cast(name);
    }

private:
    PBatch *_batch;

    int arg_timeout_ms;
    Timer _timer;
    PBatch *_timed_batch;

    int _count;
    int _drops;

    int arg_work_mode;

public:
    virtual PBatch *alloc_batch() {
	PBatch *p;
	if (!_alloc_list) {
	    if (_swap_list) {
		pthread_spin_lock(&_swap_list_lock);
		_alloc_list = _swap_list;
		_swap_list = 0;
		pthread_spin_unlock(&_swap_list_lock);
	    } else {
		if (arg_verbose > 0) {
		    snap_chatter("Have to create new batch..\n");
		}
		p = create_new_batch();
		init_batch_after_creation(p);
		goto get_out;
	    }
	}

	p = _alloc_list;
	_alloc_list = _alloc_list->list_node1;
	init_batch_for_reuse(p);
    get_out:
	return p;
    }

    /**
     * Return value:
     *       0 recycled
     *       1 shared, no action
     *       2 destroyed
     */
    virtual int kill_batch(PBatch *p) {
	if (__sync_fetch_and_sub(&p->shared, 1) == 0) {
	    finit_batch_for_recycle(p);
	    if (!recycle_batch(p)) {
		destroy_batch(p);
		return 2;
	    }
	    return 0;
	} else
	    return 1;
    }

protected:
    PBatch **_free_lists;
    int *_fl_sizes;
    PBatch *_alloc_list;
    PBatch *_swap_list;

    int arg_max_free_list_size; // Must set
    int arg_pre_alloc_size;     // Must set

    pthread_spinlock_t _swap_list_lock;
    
    virtual bool recycle_batch(PBatch *p) {
	PBatch *&free_list = _free_lists[click_current_thread_id];
	int &fsz = _fl_sizes[click_current_thread_id];
	
	if (free_list) {
	    p->list_node1 = free_list;
	    p->list_node2 = free_list->list_node2;
	    free_list->list_node2 = 0;
	    free_list = p;

	    fsz++;
	    if (fsz >= arg_max_free_list_size) {
		pthread_spin_lock(&_swap_list_lock);
		free_list->list_node2->list_node = _swap_list;
		free_list->list_node2 = 0;
		_swap_list = free_list;
		pthread_spin_unlock(&_swap_list_lock);
		
		free_list = 0;
		fsz = 0;
	    }
	} else {
	    free_list = p;
	    free_list->list_node1 = 0;
	    free_list->list_node2 = p;
	}
	return true;
    }

    virtual int destroy_batch(PBatch *p) {
	p->finit();
	if (p->host_mem)
	    g4c_free_page_lock_mem(p->host_mem);
	if (p->dev_mem)
	    g4c_free_dev_mem(p->dev_mem);
    }

    virtual PBatch *create_new_batch() {
	return new PBatch(this);
    }
    
    virtual int init_batch_after_creation(PBatch* p) {
	p->init();
	alloc_batch_priv_data(p);
	if (this->mem_size) {
	    void *hm = g4c_alloc_page_lock_mem(mem_size);
	    void *dm = g4c_alloc_dev_mem(mem_size);

	    assert(hm && dm);
	    assign_batch_mem(pb, hm, dm, mem_size);
	    
	    p->hwork_ptr = p->host_mem;
	    p->dwork_ptr = p->dev_mem;

	    if (has_annos() && r_anno_len == 0) {
		p->work_size = annos_offset;
	    } else
		p->work_size = (int)mem_size;
	}
	return 0;
    }
	
    virtual void finit_batch_for_recycle(PBatch *p) {
	if (p->dev_stream) {
	    g4c_free_stream(p->dev_stream);
	    p->dev_stream = 0;
	}
	
	for (int i=0, j=0; j<p->npkts && i<batch_size; i++) {
	    if (p->pptrs[i]) {
		p->pptrs[i]->kill();
		p->pptrs[i] = 0;
		j++;
	    }
	}
	p->npkts = 0;	
    }

    virtual void init_batch_for_reuse(PBatch *p) {
	p->shared = 0;
	p->tsed = false;
    }

    virtual int init_pb_pool() {
	int nthreads = master()->nthreads();
	
	pthread_spin_init(&_swap_list_lock);

	_free_lists = new PBatch*[nthreads];
	memset(_free_lists, 0, sizeof(PBatch*)*nthreads);

	_fl_sizes = new int[nthreads];
	memset(_fl_sizes, 0, sizeof(int)*nthreads);

	_swap_list = 0;
	_alloc_list = 0;

	int i=0;
	while (i++ < arg_pre_alloc_size) {
	    PBatch *p = create_new_batch();
	    init_batch_after_creation(p);
	    finit_batch_for_recycle(p);
	    p->list_node1 = _alloc_list;
	    _alloc_list = p;	    
	}

	snap_chatter("Pre-allocated %d batches for batcher %p\n",
		     arg_pre_alloc_size, this);
	return 0;	
    }
    
private:
    void add_packet(Packet *p);
};

CLICK_ENDDECLS
#endif
