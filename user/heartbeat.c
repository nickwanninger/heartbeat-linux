#define _GNU_SOURCE /* See feature_test_macros(7) */
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "./heartbeat.h"

static int hbfd = 0;

struct hb_info {
  // if interval is non-zero, repeat by this number
  // of microseconds on each heartbeat
  uint64_t interval;
  volatile int mask;
};

// TODO: LOCK THIS!!!
static struct hb_info info;

void hb_set_rollforwards(struct hb_rollforward *rfs, unsigned count) {
  // printf("set rollforwards!\n");
  struct hb_set_rollforwards_arg arg;
  arg.rfs = rfs;
  arg.count = count;
  ioctl(hbfd, HB_SET_ROLLFORWARDS, &arg);
}

int hb_init() {
  if (hbfd != 0)
    return -EEXIST;
  hbfd = open("/dev/heartbeat", O_RDWR);
  if (hbfd <= 0)
    return -1;
  info.interval = 0;
  info.mask = 0;
  return 0;
}

void hb_exit(void) {
  // if it hasn't been opened yet,
  if (hbfd == 0)
    return;
  close(hbfd);
  hbfd = 0;
}

// schedule the next heartbeat
static void hb_schedule(uint64_t us, int repeat) {
  struct hb_configuration config;
  config.interval = us;
  config.repeat = repeat;
  if (hbfd != 0) {
    ioctl(hbfd, HB_SCHEDULE, &config);
    printf("opened /dev/heartbeat/");
  }
}

int hb_oneshot(uint64_t us) {
  // an interval of zero means it's oneshot
  info.interval = 0;

  hb_schedule(us, 0);
  return 0;
}

int hb_repeat(uint64_t us) {
  info.interval = us;
  hb_schedule(us, 1);
  return 0;
}

int hb_cancel(void) {
  // if there is not a scheduled heartbeat, return 0
  info.interval = 0;
  hb_schedule(0, 0);
  return 1;
}
