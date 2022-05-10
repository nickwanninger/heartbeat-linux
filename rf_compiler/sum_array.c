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

int __attribute__((preserve_all, noinline)) __rf_handle_sumarray(int x) {
  my_nb_handler_calls++;
  void* ra_dst = __builtin_return_address(0);
  void* ra_src = NULL;
  // Binary search over the rollbackwards
  {
    int64_t i = 0, j = (int64_t)rollforward_table_size - 1;
    int64_t k;
    while (i <= j) {
      k = i + ((j - i) / 2);
      if ((uint64_t)rollback_table[k].from == (uint64_t)ra_dst) {
	ra_src = rollback_table[k].to;
	break;
      } else if ((uint64_t)rollback_table[k].from < (uint64_t)ra_dst) {
	i = k + 1;
      } else {
	j = k - 1;
      }
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
      __rf_handle_sumarray(proc);
      s += a[i];
    }
  }
  sums[proc] = s;
  nb_handler_calls[proc] = my_nb_handler_calls;
  return v;
}

void ping_handler(int sig, siginfo_t *si, void *priv) {
  mcontext_t *mctx = &((ucontext_t *)priv)->uc_mcontext;
  void **rip = (void**)&mctx->gregs[16];
  void* ra_src = *rip;
  void* ra_dst = NULL;
  // Binary search over the rollforwards
  {
    int64_t i = 0, j = (int64_t)rollforward_table_size - 1;
    int64_t k;
    while (i <= j) {
      k = i + ((j - i) / 2);
      if ((uint64_t)rollforward_table[k].from == (uint64_t)ra_src) {
	ra_dst = rollforward_table[k].to;
	break;
      } else if ((uint64_t)rollforward_table[k].from < (uint64_t)ra_src) {
	i = k + 1;
      } else {
	j = k - 1;
      }
    }
  }
  if (ra_dst != NULL) {
    *rip = ra_dst;
  }
}

//cpu_set_t cpuset;

void *work_ping_thread(void *v) {
  int core = (off_t)v;
  pthread_t thread = pthread_self();
  {
    sigset_t mask, prev_mask;
    if (pthread_sigmask(SIG_SETMASK, NULL, &prev_mask)) {
      exit(0);
    }
    struct sigaction sa, prev_sa;
    sa.sa_sigaction = ping_handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_mask = prev_mask;
    sigdelset(&sa.sa_mask, SIGUSR1);
    if (sigaction(SIGUSR1, &sa, &prev_sa)) {
      exit(0);
    }
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
  }
  //int s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

  sum_array(v);

  return v;
}

void ping_thread_main(int nproc) {
  /*
  CPU_ZERO(&cpuset);
  for (int j = 0; j < nproc; j++) {
    CPU_SET(j, &cpuset);
  }
  */
  for (off_t i = 0; i < nproc; i++) {
    pthread_create(&threads[i], NULL, work_ping_thread, (void *)i);
  }
  {
    unsigned int ns;
    unsigned int sec;
    struct itimerspec itval;
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
      exit(1);
    }
    {
      int one_million = 1000000;
      sec = interval_us / one_million;
      ns = (interval_us - (sec * one_million)) * 1000;
      itval.it_interval.tv_sec = sec;
      itval.it_interval.tv_nsec = ns;
      itval.it_value.tv_sec = sec;
      itval.it_value.tv_nsec = ns;
    }
    timerfd_settime(timerfd, 0, &itval, NULL);
    while (!done) {
      for (int i = 0; i < nproc; i++) {
        unsigned long long missed;
        int ret = read(timerfd, &missed, sizeof(missed));
        if (ret == -1) {
          exit(1);
        }
        pthread_kill(threads[i], SIGUSR1);
      }
    }
  }
  for (int i = 0; i < nproc; i++) {
    pthread_join(threads[i], NULL);
  }
}

void hbtimer_main(int nproc) {
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
}

int main(int argc, char **argv) {
  //printf("there are %lu rollforward entries\n", rollforward_table_size);
  // ./sum_array <array_block_len> <nproc?> <interval_us?>

  if (argc < 3) {
    printf("need at least 3 args but passed %d\n", argc);
    return 1;
  }
  array_block_len = atol(argv[1]);
  int hbtimer = argv[2][0] == 'h';
  nproc = (argc >= 4) ? atol(argv[3]) : sysconf(_SC_NPROCESSORS_ONLN);
  interval_us = (argc >= 5) ? atol(argv[4]) : interval_us;
  
  array_len = array_block_len * nproc;
  array = (uint64_t*)malloc(sizeof(uint64_t) * array_len);
  sums = (uint64_t*)malloc(sizeof(uint64_t) * nproc);
  nb_handler_calls = (uint64_t*)malloc(sizeof(uint64_t) * nproc);
  for (uint64_t i = 0; i < array_len; i++) {
    array[i] = 1;
  }

  //printf("array length = %lu, nproc = %lu\n", array_len, nproc);

  threads = (pthread_t *)malloc(sizeof(pthread_t) * nproc);

  // start alarm
  signal(SIGALRM, alarm_handler);
  alarm(1);

  if (hbtimer) {
    hbtimer_main(nproc);
  } else {
    ping_thread_main(nproc);
  }
  
  uint64_t sum = 0;
  for (int i = 0; i < nproc; i++) {
    sum += sums[i];
  }
  uint64_t result = sum;
  uint64_t handler_calls = 0;
  for (int i = 0; i < nproc; i++) {
    handler_calls += nb_handler_calls[i];
  }
  free(array);
  free(sums);
  free(threads);
  free(nb_handler_calls);

  char* cfg = hbtimer ? "hbtimer" : "ping_thread";
  printf("%s, %lu, %lu, %lu, %lu, %lu, %lu\n", cfg, nproc, result, handler_calls, array_block_len, array_len, interval_us);
  
  return 0;
}
