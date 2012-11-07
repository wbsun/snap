#ifndef __HVP_UTILS_HH__
#define __HVP_UTILS_HH__

#include <click/glue.hh>

#define hvp_chatter(...)					  \
	do {							  \
	        __hvp_chatter("@%s::%d: ", __FILE__, __LINE__);	  \
		__hvp_chatter( __VA_ARGS__);			  \
	} while(0);						  \

#endif
