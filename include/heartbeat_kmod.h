#pragma once



// This include file defines types and macros to interact with the kernel
// module and /dev/heartbeat

#include <linux/ioctl.h>


#define HB_INIT _IOW('I','I', struct hb_configuration *)


struct hb_configuration {
	// the function that the heartbeat should jump to
	unsigned long handler_address;
	// the interval, in microseconds, that the heartbeat
	// should occur at. In testing, this is only limited
	// on the lower bound by 2us
	unsigned long interval;
};
