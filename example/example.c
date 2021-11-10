#include <heartbeat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

volatile int done = 0;

unsigned long hb_cycles(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}


uint64_t time_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000 * 1000) + tv.tv_usec;
}

void callback(void) { done = 1; }


int main(int argc, char **argv) {
  int res = hb_init(0);
  for (int i = 0; 1; i++) {
    done = 0;
    uint64_t start = hb_cycles();
    hb_oneshot(100, callback);

    while (!done) {
    }

    uint64_t end = hb_cycles();
    if ((i % 1000) == 0) {
      printf("%3d: %lu\n", i, end - start);
    }
  }
  hb_exit();


  return 0;
}
