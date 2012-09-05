#include <click/config.h>
#include "batcher.hh"
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

Batcher::Batcher()
{
	_count = 0;
}

Batcher::~Batcher()
{
}

Packet *
Batcher::simple_action(Packet *p_in)
{
	WritablePacket *p = p_in->uniqueify();
	click_ip *ip = reinterpret_cast<click_ip *>(p->data());
	unsigned plen = p->length();

	ip->ip_v = 4;
	ip->ip_len = htons(plen);

	if((_count++ & 7) != 1){
		ip->ip_off = 0;
	}

	unsigned hlen = ip->ip_hl << 2;
	if(hlen < sizeof(click_ip) || hlen > plen){
		ip->ip_hl = plen >> 2;
	}

	ip->ip_sum = 0;
	ip->ip_sum = click_in_cksum((unsigned char *)ip, ip->ip_hl << 2);

	p->set_ip_header(ip, hlen);

	return p;
}

void*
Batcher::cast(const char *n)
{
	if (strcmp(n, "VElement") == 0)
		return (VElement*)this;
	else if (strcmp(n, "Batcher") == 0)
		return (Element*)this;
	else
		return 0;
}

void Batcher::vpush(int port, Vector<Packet*> *ps)
{}

Vector<Packet*> *Batcher::vpull(int port)
{}



CLICK_ENDDECLS
EXPORT_ELEMENT(Batcher)
