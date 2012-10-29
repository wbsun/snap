#ifndef CLICK_PBATCH_HH
#define CLICK_PBATCH_HH
#include <click/packet.hh>

#define CLICK_PBATCH_PACKET_BUFFER_SIZE 2048
#define CLICK_PBATH_CAPACITY 1024

CLICK_DECLS

class PBatch {
public:
	int capacity;
	int size;
	Packet **pptrs;
	
	void *hostmem;
	void *devmem;

	/*
	 * Packet memory layout on host and on device:
	 *
	 *  +----------------------------------+
	 *  |                                  |
	 *  |  N * short: packet lengths       |
	 *  |                                  |
	 *  |----------------------------------|
	 *  ~  For roundup to PAGE_SIZE        ~
	 *  |----------------------------------|
	 *  |                                  |
	 *  |  N * slice_size: packet data     |
	 *  |                                  |
	 *  |----------------------------------|
	 *  ~  For roundup to PAGE_SIZE        ~
	 *  |----------------------------------|
	 *  |                                  |
	 *  |  N * anno_size: annotation data  |
	 *  |                                  |
	 *  +----------------------------------+
	 *
	 *  Packet lengths data may not be copied if the slice length
	 *  doesn't include negative value, which means max slice.
	 *  In fact, no packet length data mem allocated at all if
	 *  slice range are all positive.
	 *
	 *  Annotation data may also not be copied if anno_flags is
	 *  0 or for write only. Particularly, if 0, no annotation data
	 *  are allocated at all.
	 */
	
	/*
	 * Packet lengths for each packet.
	 * When negative, the packet is disabled.
	 */
	short *hpktlens;
	unsigned char *hslices;
	unsigned char *hpktannos;

	// Device pointers
	short *dpktlens;
	unsigned char *dslices;
	unsigned char *dpktannos;

	/*
	 * Slice is a piece of packet data. A slice is copied from Packet's data to
	 * page-locked host memory, and copied to device. Each packet has a slice.
	 * Slice is only part of the packet data. The range is assigned by the batcher
	 * that creates the PBatch. Batcher knows slice range via device elements
	 * that use the batcher. Batcher element is assigned to device elements in
	 * configuration file. During configuring, device elements call Batcher's
	 * set_slice_range() to set the range they need. The final range may be
	 * larger than the one a device element specified because a Batcher could
	 * be shared by multiple device elements that need different ranges.
	 * But set_slice_range() will find their union without gap.
	 *
	 * Vars:
	 *   slice_begin:  begin position in packet data to form slice.
	 *   slice_end:    end position in packet data to form slice, a negative
	 *                 slice_end means copy the entire packet data.
	 *   slice_length: the finally allocated slice length, usually
	 *                 it is slice_end - slice_begin.
	 *   slice_size:   the memory allocated to hold a slice. For round up to
	 *                 DW or QW or other alignments.
	 *   anno_flags:   flags to indicate whether annotation needed, to produce
	 *                 or to use or both.
	 *   anno_size:    the memory allcoated to hold a packet annotation, for
	 *                 roundup.
	 *
	 */
	int slice_begin;
	int slice_end;
	int slice_length; // finally allocated slice length
	int slice_size; // memory size allocated to hold a slice.
	unsigned char anno_flags;
#define PBATH_ANNO_READ ((unsigned char)0x01)
#define PBATH_ANNO_WRITE ((unsigned char)0x02)
	int anno_size; 

	/*
	 * For D2H and H2D copy elements, and device execution elements.
	 *   dev_stream: stream id used for device operations.
	 *   hwork_ptr: host side work pointer, for current copy and execution data.
	 *   dwork_ptr: device side work pointer.
	 *   work_size: current work data size.
	 */
	int dev_stream;
	void *hwork_ptr;
	void *dwork_ptr;
	int work_size;

public:
	// Functions:
	PBatch();
	~PBatch();
	int init_for_host_batching();
	void clean_for_host_batching();
};

CLICK_ENDDECLS
#endif
