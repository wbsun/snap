#ifndef CLICK_VELEMENT_HH
#define CLICK_VELEMENT_HH
#include <click/element.hh>
#include <click/list.hh>
#include <click/vector.hh>
class Packet;
CLICK_DECLS

struct pktlist_node {
	Packet *pkt;
	List_member<pktlist_node> link;

	pktlist_node(Packet *v) : pkt(v) {}
};

typedef List<pktlist_node, &pktlist_node::link> pktlist;

class VElement : public Element {
public:
	VElement();
	virtual ~VElement();

	virtual void vpush(int port, Vector<Packet*> *ps) = 0;
	virtual Vector<Packet*> *vpull(int port) = 0;

	virtual const int batch_size() const = 0;
};

CLICK_ENDDECLS
#endif
