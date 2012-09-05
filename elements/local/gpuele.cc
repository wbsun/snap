#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

GPUEle::GPUEle()
{
	_count = 0;
	_pktbufsz = DEFAULT_PACKET_BUF_SZ;
	_batchsz = DEFAULT_BATCH_SZ;
	_ustimeout = DEFAULT_BATCH_TIMEOUT;
}

GPUEle::~GPUEle()
{
}

int
GPUEle::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh,
			 "BATCHSZ", 0, cpUnsigned, &_batchsz,
			 "PKTBUFSZ", 0, cpSize, &_pktbufsz,
			 "TIMEOUT", 0, cpUnsigned, &_ustimeout,
			 cpEnd) < 0) return -1;
	if (_pktbufsz < 1540) // Need a real common MTU to figure out this
		return errh->error("packet buffer size is %u, too small\n", _pktbufsz);

	return 0;
}

PacketBatch
GPUEle::init_batch()
{
	PacketBatch pb;
	pb.first = new PacketVector();
	if (pb.first) {
		if (pb.first->reserve(_batchsz)) {
			pb.second = g4c_malloc(_batchsz*_pktbufsz);
			if (pb.second)
				return pb;
			
			click_chatter("g4c malloc failed, sz %uB\n", _batchsz*_pktbufsz);
		} else
			click_chatter("can't reserve %d items space for packet batch\n", _batchsz);

		delete pb.first;
		pb.first = 0;
	} else
		click_chatter("new packet vecotr allocation failed\n");

	return pb;
}

int
GPUEle::initialize(ErrorHandler *errh)
{
	errh->message("initializing GPUEle...");

	if (!_batches.reserve(MAX_NR_BATCH)) {
		click_chatter("batches failed to reserve space\n");
		return -ENOMEM;
	}

	_curbatch = init_batch();
	if (!_curbatch.first)
		return -ENOMEM;
	
	return 0;
}

void
GPUEle::run_timer(Timer *timer)
{
}

bool
GPUEle::run_task(Task *task)
{
}


Packet *
GPUEle::pull(int i)
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

CLICK_ENDDECLS
EXPORT_ELEMENT(GPUEle)
