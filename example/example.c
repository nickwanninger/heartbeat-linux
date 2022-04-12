#define _GNU_SOURCE
#include <heartbeat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/timerfd.h>

__thread volatile uint64_t nb_iters = 0;
volatile int done = 0;
int nproc = 0;
pthread_t* threads;

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

static
cpu_set_t cpuset;

uint64_t per_worker_iters[128];

extern long hb_test(void);
extern void hb_test_target(void);
extern void hb_test_target_rf(void);

volatile int live_threads = 0;

void ping_handler(int sig, siginfo_t *si,  void *priv) {
  mcontext_t* mctx = &((ucontext_t *)priv)->uc_mcontext;
  void** rip = &mctx->gregs[16];
  if (*rip == hb_test_target) {
    *rip = hb_test_target_rf;
  }
}

void work(int i) {
  __sync_fetch_and_add(&live_threads, 1);
  for (int i = 0; !done; i++) {
    nb_iters += hb_test();
  }
  __sync_fetch_and_add(&live_threads, -1);
  //printf("done\n");
  per_worker_iters[i] = nb_iters;
}

void *work_hbtimer(void *v) {
  int core = (off_t)v;
  if (core == 0) {
    return NULL;
  }
  //printf("starting on core %d\n", core);
  int res = hb_init(core);

  struct hb_rollforward rf;
  rf.from = hb_test_target;
  rf.to = hb_test_target_rf;
  hb_set_rollforwards(&rf, 1);
  hb_repeat(interval, NULL);
  work(core);
  hb_exit();

  return v;
}

pthread_barrier_t ping_thread_barrier;

void *work_ping_thread(void *v) {
  int core = (off_t)v;
  pthread_t  thread = pthread_self();
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
  pthread_barrier_wait(&ping_thread_barrier);
  int s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (core == 0) {
    unsigned int ns;
    unsigned int sec;
    struct itimerspec itval;
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
      exit(1);
    }
    {
      auto one_million = 1000000;
      sec = interval / one_million;
      ns = (interval - (sec * one_million)) * 1000;
      itval.it_interval.tv_sec = sec;
      itval.it_interval.tv_nsec = ns;
      itval.it_value.tv_sec = sec;
      itval.it_value.tv_nsec = ns;
    }
    timerfd_settime(timerfd, 0, &itval, NULL);

    while (!done || (live_threads != 0)) {
      for (int i = 0; i < nproc; i++) {
	unsigned long long missed;
	int ret = read(timerfd, &missed, sizeof(missed));
	if (ret == -1) {
	  exit(1);
	}
	pthread_kill(threads[i], SIGUSR1);
      }
    }
    return NULL;
  }
  //  printf("starting on core %d\n", core);
  if (s != 0)
    printf("error\n");

  work(core);

  return v;
}

void hbtimer_main(int nproc) {
  for (off_t i = 0; i < nproc; i++) {
    pthread_create(&threads[i], NULL, work_hbtimer, (void *)i);
  }

  for (int i = 0; i < nproc; i++) {
    pthread_join(threads[i], NULL);
  }
}

void ping_thread_main(int nproc) {
  CPU_ZERO(&cpuset);
  for (int j = 0; j < nproc; j++) {
    CPU_SET(j, &cpuset);
  }
  
  for (off_t i = 0; i < nproc; i++) {
    pthread_create(&threads[i], NULL, work_ping_thread, (void *)i);
  }
  
  for (int i = 0; i < nproc; i++) {
    pthread_join(threads[i], NULL);
  }
  
}

void handler() {
  done = 1;
}

int main(int argc, char **argv) {
  nproc = sysconf(_SC_NPROCESSORS_ONLN);
  pthread_barrier_init(&ping_thread_barrier, NULL, nproc);
  threads = (pthread_t*)malloc(sizeof(pthread_t) * nproc);
  for (int i = 0; i < 128; i++) {
    per_worker_iters[i] = 0;
  }
  // start alarm
  signal(SIGALRM, handler);
  signal(SIGINT, handler);
  alarm(2);
  interval = atoi(argv[2]);
  int hbtimer = argv[1][0] == 'h';
  printf("interval %lu\n", interval);
  if (hbtimer) {
    printf("hbtimer\n");
    hbtimer_main(nproc);
  } else {
    printf("ping\n");
    ping_thread_main(nproc);
  }
  uint64_t all_iters = 0;
  for (int i = 0; i < 128; i++) {
    all_iters += per_worker_iters[i];
  }
  free(threads);
  printf("iters %lu\n", all_iters);
  printf("iters_per_proc %lu\n", all_iters/nproc);
  return 0;
}
