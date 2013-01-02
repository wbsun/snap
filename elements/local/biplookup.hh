#ifndef CLICK_BIPLOOKUP_HH
#define CLICK_BIPLOOKUP_HH
#include <click/glue.hh>
#include <click/element.hh>
#include "../ip/iproutetable.hh"
#include <click/pbatch.hh>
#include "batcher.hh"
#include <g4c.h>
#include <g4clookup.h>
#include <vector>
using namespace std;

CLICK_DECLS

class BIPLookup : public IPRouteTable {
public:
    BIPLookup();
    ~BIPLookup();
    
    const char *class_name() const	{ return "BIPLookup"; }
    const char *port_count() const	{ return "1/1"; }
    const char *processing() const  { return PUSH; }

    void bpush(int i, PBatch *p);
    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

    virtual int add_route(const IPRoute& route, bool allow_replace,
			  IPRoute* replaced_route, ErrorHandler* errh);
    virtual int remove_route(const IPRoute& route, IPRoute* removed_route, ErrorHandler* errh);
    virtual int lookup_route(IPAddress addr, IPAddress& gw);
    virtual String dump_routes();

    int build_lpmt(vector<g4c_ipv4_rt_entry> &rtes, g4c_lpm_tree *&hlpmt,
		   g4c_lpm_tree *&dlpmt, int nbits, size_t &tsz, ErrorHandler *errh);

private:
    Batcher* _batcher;
    vector<g4c_ipv4_rt_entry> _rtes;
    bool _test;
    g4c_lpm_tree *_hlpmt, *_dlpmt;
    uint32_t _lpmt_lock;
    int _lpm_bits;
    size_t _lpm_size;
    PSliceRange _psr;
    int16_t _anno_offset;
    int16_t _slice_offset;
};
CLICK_ENDDECLS
#endif
