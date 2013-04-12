#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/hvputils.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <arpa/inet.h>
#include <sys/time.h>
#include <click/atomic.hh>
#include <fstream>
using namespace std;
#include "snapclassifier.hh"

CLICK_DECLS

SnapClassifier::SnapClassifier()
{
    _debug = false;
    gcl = 0;
}

SnapClassifier::~SnapClassifier()
{
}

static bool
read_pattern_file(String &filename, Vector<String> &sptns, ErrorHandler *errh)
{
    ifstream ifs (filename.c_str());
    if (ifs.fail()) {
	errh->error("Failed to open %s", filename.c_str());
	return false;
    }

    sptns.reserve(ifs.rdbuf()->in_avail()/80); // simple restimation of # lines

    char pattern[256]; // yes, yes, I know, I know..
    do {
	ifs.getline(pattern, 256);
	sptns.push_back(String(pattern));
    } while (!ifs.eof());

    ifs.close();
    return true;
}

static void
dump_patterns(g4c_pattern_t *ptns, int n)
{
    for (int i=0; i<n; i++) {
	click_chatter("0X%X, %u, 0X%X, %u, %u, %u, 0X%X ",
		      ptns[i].src_addr, ptns[i].nr_src_netbits,
		      ptns[i].dst_addr, ptns[i].nr_dst_netbits,
		      ptns[i].src_port, ptns[i].dst_port,
		      ptns[i].proto);
    }
}

int
SnapClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String filename;
    bool fromfile;
    int nr_patterns = 1000;
    
    if (cp_va_kparse(conf, this, errh,
		     "PATTERN_FILE", cpkC, &fromfile, cpFilename, &filename,
		     "NR_PATTERNS", cpkN, cpInteger, &nr_patterns,
		     "DEBUG", cpkN, cpBool, &_debug,
		     cpEnd) < 0)
	return -1;

    g4c_pattern_t *ptns = 0;    
    if (fromfile) {
	Vector<String> sptns;
	if (!read_pattern_file(filename, sptns, errh))
	    return -1;

	if (_debug)
	    nr_patterns = sptns.size();
	
	ptns = (g4c_pattern_t*)malloc(sizeof(g4c_pattern_t)*sptns.size());
	if (!ptns) {
	    errh->error("failed to alloc mem for patterns");
	    return -1;
	}
	memset(ptns, 0, sizeof(g4c_pattern_t)*sptns.size());
	if (!parse_patterns(sptns, errh, ptns, sptns.size())) {
	    errh->error("failed to parse patterns");
	    return -1;
	}
    } else {
	ptns = (g4c_pattern_t*)malloc(sizeof(g4c_pattern_t)*nr_patterns);
	if (!ptns) {
	    errh->error("failed to alloc mem for patterns");
	    return -1;
	}
	
	memset(ptns, 0, sizeof(g4c_pattern_t)*nr_patterns);
	generate_random_patterns(ptns, nr_patterns);
    }

    /*if (_debug)
      dump_patterns(ptns, nr_patterns);*/

    free(ptns);

    return 0;
}

static bool
tokenize_filter(String& line, const char *&cursor, ErrorHandler *errh)
{
    while(cursor != line.end() && *cursor != ' ')
	cursor++;
    if (cursor == line.end()) {
	errh->error("Bad formart of Classifier filter");
	return false;
    }
    return true;
}

bool
SnapClassifier::parse_patterns(Vector<String> &conf, ErrorHandler *errh,
			       g4c_pattern_t *ptns, int n)
{
    // format:
    // srcip/mask dstip/mask lowsrcport : highsrcport lowdstport : highdstport protovalue/protomask
    for (int i=0; i<conf.size(); i++) {
	ptns[i].idx = i;
	const char *begin = conf[i].begin();
	const char *cursor = begin;

	// 0 srcip, 1 dstip, 2 lsp, 4 hsp, 5 ldp, 7 hdp, 8 protovalue, 9 protomask
	String tokens[10];

	for (int j=0; j<8; j++) {
	    if (!tokenize_filter(conf[i], cursor, errh))
		return false;
	    tokens[j] = String(begin, cursor);
	    cursor++;
	    begin = cursor;
	}

	while(cursor != conf[i].end() && *cursor != '/')
	    cursor++;
	tokens[8] = String(begin, cursor);
	begin = cursor+1;

	while(cursor != conf[i].end())
	    cursor++;
	tokens[9] = String(begin, cursor);

	/*if (_debug) {
	    click_chatter(" %s | %s | %s | %s | %s | %s | %s | %s",
			  tokens[0].c_str(), tokens[1].c_str(),
			  tokens[2].c_str(), tokens[4].c_str(),
			  tokens[5].c_str(), tokens[7].c_str(),
			  tokens[8].c_str(), tokens[9].c_str());
			  }*/

	IPAddress src[2], dst[2];
	/*if (_debug) {
	    bool rt = IPPrefixArg(true).parse(tokens[0], src[0], src[1]);
	    bool rth = IPPrefixArg().parse(String("10.1.1.2/255.255.0.0"), dst[0], dst[1]);
	    click_chatter("parse %s get %s %s %s\n",
			  tokens[0].c_str(), rt?"true":"false",
			  src[0].unparse().c_str(),
			  src[1].unparse().c_str());
	    click_chatter("parse 10.1.1.2/255.255.0.0 get %s %s %s\n",
			  rth?"true":"false",
			  dst[0].unparse().c_str(),
			  dst[1].unparse().c_str());
			  }*/
	IPPrefixArg(true).parse(tokens[0], src[0], src[1]);
	IPPrefixArg(true).parse(tokens[1], dst[0], dst[1]);

	ptns[i].src_addr = src[0].addr();
	ptns[i].nr_src_netbits = src[1].mask_to_prefix_len();
	
	ptns[i].dst_addr = dst[0].addr();
	ptns[i].nr_dst_netbits = dst[1].mask_to_prefix_len();

	unsigned int lsp, hsp, ldp, hdp, ptv, ptm;

	IntArg().parse(tokens[2], lsp);
	IntArg().parse(tokens[4], hsp);
	IntArg().parse(tokens[5], ldp);
	IntArg().parse(tokens[7], hdp);	
	IntArg().parse(tokens[8], ptv);
	IntArg().parse(tokens[9], ptm);

	if (lsp == 0 && hsp == 65535)
	    ptns[i].src_port = -1;
	else
	    ptns[i].src_port = hsp;
	
	if (ldp == 0 && hdp == 65535)
	    ptns[i].dst_port = -1;
	else
	    ptns[i].dst_port = hdp;
	
	if (ptm == 0)
	    ptns[i].proto = -1;
	else
	    ptns[i].proto = ptv;
    }
    return true;
}

void
SnapClassifier::generate_random_patterns(g4c_pattern_t *ptns, int n)
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    srandom((unsigned)(tv.tv_usec));

    for (int i=0; i<n; i++) {
	int nbits = random()%5;
	if (random()%3) {
	    ptns[i].nr_src_netbits = nbits*8;
	    for (int j=0; j<nbits; j++)
		ptns[i].src_addr = (ptns[i].src_addr<<8)|(random()&0xff);
	} else
	    ptns[i].nr_src_netbits = 0;
	
	if (random()%3) {
	    nbits = random()%5;
	    ptns[i].nr_dst_netbits = nbits*8;
	    for (int j=0; j<nbits; j++)
		ptns[i].dst_addr = (ptns[i].dst_addr<<8)|(random()&0xff);
	} else
	    ptns[i].nr_dst_netbits = 0;
	
	if (random()%3) {
	    ptns[i].src_port = random()%(PORT_STATE_SIZE<<1);
	    if (ptns[i].src_port >= PORT_STATE_SIZE)
		ptns[i].src_port -= PORT_STATE_SIZE*2;
	} else
	    ptns[i].src_port = -1;
	
	if (random()%3) {
	    ptns[i].dst_port = random()%(PORT_STATE_SIZE<<1);
	    if (ptns[i].dst_port >= PORT_STATE_SIZE)
		ptns[i].dst_port -= PORT_STATE_SIZE*2;
	} else
	    ptns[i].dst_port = -1;
	
	if (random()%3) {
	    ptns[i].proto = random()%(PROTO_STATE_SIZE);
	} else
	    ptns[i].proto = -1;
	ptns[i].idx = i;
    }
}

int
SnapClassifier::initialize(ErrorHandler *errh)
{
    /*
    if (_on_cpu == 3)
	return 0;
    
    g4c_pattern_t *ptns = 0;
    int nptns = 0;
    if (_test > 2) {
	ptns = (g4c_pattern_t*)malloc(sizeof(g4c_pattern_t)*_test);
	if (!ptns) {
	    errh->error("Failed to alloc mem for patterns");
	    return -1;
	}
	memset(ptns, 0, sizeof(g4c_pattern_t)*_test);
	generate_random_patterns(ptns, _test);
	nptns = _test;
    } 
    
    int s = g4c_alloc_stream();
    if (!s) {
	errh->error("Failed to alloc stream for classifier copy");
	return -1;
    }

    gcl = g4c_create_classifier(ptns, nptns, 1, s);
    if (!gcl || !gcl->devmem) {
	errh->error("Failed to create classifier");
	if (_test > 2) {
	    g4c_free_stream(s);
	    free(ptns);
	}
	return -1;
    } else {
	errh->message("Classifier built for host and device.");
	if (_test > 2)
	    free(ptns);
    }

    g4c_free_stream(s);

    if (_on_cpu < 2) {
	_batcher->setup_all();
	_anno_offset = _batcher->get_anno_offset(0);
	if (_anno_offset < 0) {
	    errh->error("Failed to get anno offset in batch "
			"anno start %u, anno len %u "
			"w start %u, w len %u",
			_batcher->anno_start, _batcher->anno_len,
			_batcher->w_anno_start, _batcher->w_anno_len);
	    return -1;
	} else
	    errh->message("SnapClassifier anno offset %d", _anno_offset);

	_slice_offset = _batcher->get_slice_offset(_psr);
	if (_slice_offset < 0) {
	    errh->error("Failed to get slice offset in batch ranges:");
	    for (int i=0; i<_batcher->nr_slice_ranges; i++) {
		errh->error("start %d, off %d, len %d, end %d",
			    _batcher->slice_ranges[i].start,
			    _batcher->slice_ranges[i].start_offset,
			    _batcher->slice_ranges[i].len,
			    _batcher->slice_ranges[i].end);
	    }
	    return -1;
	} else
	    errh->message("SnapClassifier slice offset %d", _slice_offset);
    } else {
	_anno_offset = 0;
	_slice_offset = 22; // IP dst
    }
    */
	
    return 0;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(SnapClassifier)
ELEMENT_LIBS(-lg4c)    
