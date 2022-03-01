#define _GNU_SOURCE
#include <heartbeat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

__thread volatile int done = 0;

unsigned long hb_cycles(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}

extern void dumb(void);


uint64_t time_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000 * 1000) + tv.tv_usec;
}



long interval = 100;
void callback(hb_regs_t *regs) { done = 1; }

extern long hb_test(void);
extern void hb_test_target(void);
extern void hb_test_target_rf(void);


void *work(void *v) {
  int core = (off_t)v;
  printf("starting on core %d\n", core);
  int res = hb_init(core);

  struct hb_rollforward rf;
  rf.from = hb_test_target;
  rf.to = hb_test_target_rf;
  hb_set_rollforwards(&rf, 1);
  hb_repeat(interval, NULL);
  for (int i = 0; 1; i++) {
    uint64_t start = time_us();
    long iters = hb_test();
    uint64_t end = time_us();
    printf("%3d: %lu %ld\n", i, end - start, iters);
    done = 0;
  }
  hb_exit();

  return v;
}

int main(int argc, char **argv) {
  int nproc = sysconf(_SC_NPROCESSORS_ONLN);
  pthread_t threads[nproc];

  for (off_t i = 0; i < nproc; i++) {
    pthread_create(&threads[i], NULL, work, (void *)i);
  }

  for (int i = 0; i < nproc; i++) {
    pthread_join(threads[i], NULL);
  }



  return 0;
}
