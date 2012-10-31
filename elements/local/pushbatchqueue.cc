#include <click/config.hh>
#include "pushbatchqueue.hh"
#include <click/error.hh>
#include <click/hvputils.hh>
#include "batcher.hh"
CLICK_DECLS



CLICK_ENDDECLS
ELEMENT_REQUIRES(Batcher)
EXPORT_ELEMENT(PushBatchQueue)
ELEMENT_LIBS(-lg4c)
