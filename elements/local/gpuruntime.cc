#include <click/config.h>
#include "gpuruntime.hh"
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <g4c.h>
CLICK_DECLS

GPURuntime::GPURuntime() {
	_hostmem_sz = G4C_DEFAULT_MEM_SIZE;
	_devmem_sz = G4C_DEFAULT_MEM_SIZE;
	_nr_streams = G4C_DEFAULT_NR_STREAMS;
}

GPURuntime::~GPURuntime() {}

int
GPURuntime::configure(Vector<String> &conf, ErrorHandler* errh)
{
	int ns = 0;
	size_t hsz = 0, dsz = 0;
	
	if (cp_va_kparse(conf, this, errh,
			 "STREAMS", cpkN, cpInteger, &ns,
			 "HOSTMEMSZ", cpkN, cpSize, &hsz,
			 "DEVMEMSZ", cpkN, cpSize, &dsz,
			 cpEnd) < 0)
		return -1;

	if (ns)
		_nr_streams = ns;
	if (hsz)
		_hostmem_sz = hsz;
	if (dsz)
		_devmem_sz = dsz;

	if (g4c_init(_nr_streams, _hostmem_sz, _devmem_sz)) {
		errh->error("GPURuntime G4C initialization failed!\n");
		hvp_chatter("GPURuntime G4C initialization failed!\n");
		return -1;
	}

	hvp_chatter("G4C GPU runtime initialized.\n");
	return 0;
}

void
GPURuntime::cleanup(CleanupStage stage)
{
	if (stage == CLEANUP_CONFIGURED ||
	    stage == CLEANUP_INITIALIZE_FAILED ||
	    stage == CLEANUP_INITIALIZED ||
	    stage == CLEANUP_ROUTER_INITIALIZED ) {		
		g4c_exit();
		hvp_chatter("G4C GPU runtime cleaned up.\n");
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPURuntime)
ELEMENT_LIBS(-lg4c)
