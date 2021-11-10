#define _GNU_SOURCE /* See feature_test_macros(7) */
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sched.h>

#include <heartbeat.h>

// This is the entrypoint for the heartbeat from kernelspace.
// defined in heartbeat.S
extern void _hb_entry(void);

static __thread int hbfd = 0;

struct hb_thread_info {
  // if interval is non-zero, repeat by this number
  // of microseconds on each heartbeat
  uint64_t interval;
  hb_callback_t callback;
  volatile int mask;
};

static __thread struct hb_thread_info info;


void hb_entry_high() {

	if (info.mask) return;


	info.mask = 1;
	if (info.callback)
		info.callback();
	info.mask = 0;
}



int hb_init(int cpu) {
  if (hbfd != 0) return -EEXIST;
  hbfd = open("/dev/heartbeat", O_RDWR);

  cpu_set_t my_set;                                 /* Define your cpu_set bit mask. */
  CPU_ZERO(&my_set);                                /* Initialize it all to 0, i.e. no CPUs selected. */
  CPU_SET(cpu, &my_set);                            /* set the bit that represents core 7. */
  sched_setaffinity(0, sizeof(cpu_set_t), &my_set); /* Set affinity of tihs process to */
  sched_yield();

  if (hbfd <= 0) return -1;
  info.callback = NULL;
  info.interval = 0;
  info.callback = _hb_entry;
	info.mask = 0;
  return 0;
}

void hb_exit(void) {
  // if it hasn't been opened yet,
  if (hbfd == 0) return;
  close(hbfd);
  hbfd = 0;
}


// schedule the next heartbeat
static void hb_schedule(uint64_t us, int repeat) {
  struct hb_configuration config;
  config.interval = us;
  config.handler_address = (off_t)_hb_entry;
	config.repeat = repeat;
  if (hbfd != 0) {
    ioctl(hbfd, HB_SCHEDULE, &config);
  }
  // TODO:
}


int hb_oneshot(uint64_t us, hb_callback_t callback) {
  // an interval of zero means it's oneshot
  info.interval = 0;
  info.callback = callback;

  hb_schedule(us, 0);
  return 0;
}


int hb_repeat(uint64_t us, hb_callback_t callback) {
  info.interval = us;
  info.callback = callback;
  hb_schedule(us, 1);
  return 0;
}


int hb_cancel(void) {
  // if there is not a scheduled heartbeat, return 0
  if (info.callback == NULL) return 0;

  info.callback = NULL;
  info.interval = 0;

  // scheduling a time of zero is a special
  // case that cancels timers.
  hb_schedule(0, 0);

  return 1;
}