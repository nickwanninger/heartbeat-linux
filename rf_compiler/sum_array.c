#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <string.h>
#include <stdlib.h>
#include <heartbeat.h>
#include "../src/heartbeat.c"

extern
uint64_t rollforward_table_size;
extern
struct hb_rollforward rollforward_table[];
extern
struct hb_rollforward rollback_table[];
long interval_us = 100;

volatile
int done = 0;
void alarm_handler() { done = 1; }

__thread volatile
uint64_t my_prev_handler_timestamp = 0;

uint64_t cycles(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}

__thread volatile
uint64_t my_nb_handler_calls = 0;

int __attribute__((preserve_all, noinline)) sahandler(int x) {
  my_nb_handler_calls++;
  void* ra_dst = __builtin_return_address(0);
  void* ra_src = NULL;
  for (uint64_t i = 0; i < rollforward_table_size; i++) {
    void* ra = rollback_table[i].from;
    if ((uint64_t)ra == (uint64_t)ra_dst) {
      ra_src = ra;
      break;
    }
  }
  if (ra_src != NULL) {
    void* fa = __builtin_frame_address(0);
    void** rap = (void**)((char*)fa + 8);
    *rap = ra_src;
  }
  return 0;
}

uint64_t nproc;
pthread_t* threads;

uint64_t array_block_len;
uint64_t array_len;
uint64_t* array;
uint64_t* sums;
uint64_t* nb_handler_calls;

void* sum_array(void* v) {
  int proc = (off_t)v;
  uint64_t s = 0;
  while (! done) {
    uint64_t* a = &array[proc * array_block_len];
    for (uint64_t i = 0; i < array_block_len; i++) {
      sahandler(proc);
      s += a[i];
    }
  }
  sums[proc] = s;
  nb_handler_calls[proc] = my_nb_handler_calls;
  return v;
}

int main(int argc, char **argv) {
  printf("there are %lu rollforward entries\n", rollforward_table_size);

  nproc = sysconf(_SC_NPROCESSORS_ONLN)/2;
  array_block_len = atol(argv[1]);
  array_len = array_block_len * nproc;
  array = (uint64_t*)malloc(sizeof(uint64_t) * array_len);
  sums = (uint64_t*)malloc(sizeof(uint64_t) * nproc);
  nb_handler_calls = (uint64_t*)malloc(sizeof(uint64_t) * nproc);
  for (uint64_t i = 0; i < array_len; i++) {
    array[i] = 1;
  }

  printf("array length = %lu, nproc = %lu\n", array_len, nproc);

  threads = (pthread_t *)malloc(sizeof(pthread_t) * nproc);

  // start alarm
  signal(SIGALRM, alarm_handler);
  alarm(1);

  hb_init();
  hb_set_rollforwards(&rollforward_table[0], rollforward_table_size);
  hb_repeat(interval_us, NULL);
  
  for (off_t i = 0; i < nproc; i++) {
    pthread_create(&threads[i], NULL, sum_array, (void *)i);
  }
  for (int i = 0; i < nproc; i++) {
    pthread_join(threads[i], NULL);
  }
  hb_exit();
  uint64_t sum = 0;
  for (int i = 0; i < nproc; i++) {
    sum += sums[i];
  }
  printf("sum = %lu\n", sum);
  uint64_t handler_calls = 0;
  for (int i = 0; i < nproc; i++) {
    handler_calls += nb_handler_calls[i];
  }
  printf("handler calls = %lu\n", handler_calls);
  free(array);
  free(sums);
  free(threads);
  free(nb_handler_calls);
  
  return 0;
}
