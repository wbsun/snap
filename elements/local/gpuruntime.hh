#ifndef CLICK_GPU_RUNTIME_HH
#define CLICK_GPU_RUNTIME_HH
#include <click/element.hh>
CLICK_DECLS

class GPURuntime : public Element
{
public:
    GPURuntime();
    ~GPURuntime();

    const char *class_name() const	{ return "GPURuntime"; }
    int configure_phase() const	{ return CONFIGURE_PHASE_INFO; }
    int configure(Vector<String>&, ErrorHandler*);
    void cleanup(CleanupStage stage);

    void *malloc_host(size_t sz, bool wc=false);
    void free_host(void *p);
    void *malloc_dev(size_t sz);
    void free_dev(void *p);
    int alloc_stream();
    void release_stream(int s);
    int check_stream_done(int s, bool blocking=false);
    int memcpy_h2d(void *h, void *d, size_t sz, int s);
    int memcpy_d2h(void *d, void *h, size_t sz, int s);
    
private:
    size_t _hostmem_sz;
    size_t _devmem_sz;
    size_t _wcmem_sz;
    int _nr_streams;
    bool _use_packetpool;
    bool _test;
};

CLICK_ENDDECLS

#endif
