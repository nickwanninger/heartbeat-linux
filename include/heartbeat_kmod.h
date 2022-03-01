#pragma once



// This include file defines types and macros to interact with the kernel
// module and /dev/heartbeat

#include <linux/ioctl.h>


#define HB_SCHEDULE _IOW('I', 'I', struct hb_configuration *)
#define HB_SET_ROLLFORWARDS _IOW('I', 'R', struct hb_configuration *)


struct hb_rollforward {
  void *from;  // go from here
  void *to;    // to here
};

struct hb_set_rollforwards_arg {
  struct hb_rollforward *rfs;
  unsigned count;
};

struct hb_configuration {
  // the function that the heartbeat should jump to
  unsigned long handler_address;
  // the interval, in microseconds, that the heartbeat
  // should occur at. In testing, this is only limited
  // on the lower bound by 2us
  unsigned long interval;

  // repeat in the kernel instead of waiting for a oneshot.
  // This is a little dangerous on small intervals, as your callback
  // may take longer than expected and will need to be small and re-entrant
  // TODO: how do we want to cancel?
  int repeat;
};
