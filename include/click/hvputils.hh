#ifndef __HVP_UTILS_HH__
#define __HVP_UTILS_HH__

#define hvp_chatter(...)					  \
	do {							  \
		click_chatter("At %s::%d: ", __FILE__, __LINE__); \
		click_chatter(__VA_ARGS__);			  \
	}							  \

#endif
