#include <click/config.hh>
#include "bunqueue.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "batcher.hh"
CLICK_DECLS



CLICK_ENDDECLS
ELEMENT_REQUIRES(Batcher)
EXPORT_ELEMENT(Bunqueue)
ELEMENT_LIBS(-lg4c)
