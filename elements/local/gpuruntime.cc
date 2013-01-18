#include <click/config.h>
#include "gpuruntime.hh"
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/packet.hh>
#include <g4c.h>
#include <g4c_ac.h>
#include <g4c_lpm.h>
CLICK_DECLS

GPURuntime::GPURuntime() {
    _hostmem_sz = G4C_DEFAULT_MEM_SIZE;
    _devmem_sz = G4C_DEFAULT_MEM_SIZE+G4C_DEFAULT_WCMEM_SIZE;
    _nr_streams = G4C_DEFAULT_NR_STREAMS;
    _wcmem_sz = G4C_DEFAULT_WCMEM_SIZE;
    _use_packetpool = true;
    _test = false;
}

GPURuntime::~GPURuntime() {}

int
GPURuntime::configure(Vector<String> &conf, ErrorHandler* errh)
{
    int ns = 0;
    size_t hsz = 0, dsz = 0, wcsz = 0;
    bool upp = true;

    if (!conf.size()) {
	errh->error("Need arg entries");
	return -1;
    }

    int nargs;
    if (!cp_integer(conf[0], &nargs)) {
	errh->error("First argument must be integer to specify # of general args");
	return -1;
    }

    Vector<String> g4c_args;
    Vector<String> myconf;

    myconf.reserve(nargs);
    g4c_args.reserve(conf.size());

    int i;
    for (i=1; i<=nargs; i++)
	myconf.push_back(conf[i]);
    for (; i<conf.size(); i++)
	g4c_args.push_back(conf[i]);
	
    if (cp_va_kparse(myconf, this, errh,
		     "STREAMS", cpkN, cpInteger, &ns,
		     "HOSTMEMSZ", cpkN, cpSize, &hsz,
		     "DEVMEMSZ", cpkN, cpSize, &dsz,
		     "WCMEMSZ", cpkN, cpSize, &wcsz,
		     "USEPKTPOOL", cpkN, cpBool, &upp,
		     "TEST", cpkN, cpBool, &_test,
		     cpEnd) < 0)
	return -1;

    if (ns)
	_nr_streams = ns;
    if (hsz)
	_hostmem_sz = hsz<<20;
    if (dsz)
	_devmem_sz = dsz<<20;
    if (wcsz)
	_wcmem_sz = wcsz<<20;
    
    _use_packetpool = upp;

    if (g4c_init(_nr_streams, _hostmem_sz, _wcmem_sz, _devmem_sz)) {
	errh->error("GPURuntime G4C initialization failed!\n");
	return -1;
    }

    if (g4c_args.size() & 0x1) {
	errh->error("G4C args must be in pair, now # is %d.", g4c_args.size());
	return -1;
    }
    
    int *vals = new int[g4c_args.size()];
    char *keys = new char[g4c_args.size()];
    if (!vals || !keys) {
	errh->error("out of memory for g4c args.");
	return -1;
    }

    String s;
    for (i=0; i<g4c_args.size(); i++) {
	if (i&0x1) {
	    if (!cp_integer(g4c_args[i], vals+(i>>1))) {
		errh->error("Format error: %s should be integer", g4c_args[i].c_str());
		return -1;
	    }
	} else {
	    if (!cp_string(g4c_args[i], &s)) {
		errh->error("Format error: %s should be string", g4c_args[i].c_str());
		return -1;
	    }
	    keys[i>>1] = s.c_str()[0];
	}
    }

    g4c_lut_init(g4c_args.size()>>1, keys, vals);

    if (_test) {
	errh->message("G4C initialized with these special args:\n");
	for (i=0; i<g4c_args.size()>>1; i++) {
	    errh->message("%c -> %d\n", keys[i], vals[i]);
	}
    }

    if (_use_packetpool)
	WritablePacket::pool_initialize();

    hvp_chatter("G4C GPU runtime initialized.\n");
    return 0;
}

void
GPURuntime::cleanup(CleanupStage stage)
{
    if (stage >= CLEANUP_CONFIGURED) {		
	//g4c_exit(); //comment for reducing errors..
	hvp_chatter("G4C GPU runtime cleaned up.\n");
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPURuntime)
ELEMENT_LIBS(-lg4c)
