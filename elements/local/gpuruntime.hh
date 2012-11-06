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
private:
	size_t _hostmem_sz;
	size_t _devmem_sz;
	int _nr_streams;
};

CLICK_ENDDECLS

#endif
