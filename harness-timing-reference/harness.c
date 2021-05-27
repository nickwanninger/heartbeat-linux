/*
 *
 *  Producer-Consumer Lab
 *
 *  Copyright (c) 2019 Peter A. Dinda
 *
 *  Warning: This file will be overwritten before grading.
 */


#define _GNU_SOURCE
#include <sched.h>    // for processor affinity
#include <unistd.h>   // unix standard apis
#include <pthread.h>  // pthread api
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <math.h>
#include <limits.h>

#include "config.h"
#include "ring.h"
#include "atomics.h"


struct timeval start;
struct timeval end;


#define EXTERMINATE -1ULL

pthread_barrier_t barrier;

#define TIMING_METHOD 0 //0 for rdtsc, 1 for gettimeofday
int interrupts = 1;
uint64_t interrupt_us = 100; // 10 ms by default
uint64_t num_threads;

// in order to match up with gdb:
// tid[0] => nothing
// tid[1] => main
// tid[2] => 1st producer thread
#define THREAD_OFFSET 2
pthread_t* tid;   // array of thread ids

// array of timer ids, if used - timer[0] => first interrupt producer
// we need to use timer_create, etc to actually do per-thread timers
timer_t* timer;


#define AMT_WORK 1000000
uint64_t* last; //array of last interrupt time
uint64_t* num_interrupts; //array of interrupt count for each thread
int num_cpus;


// BEGIN code for recording individual intervals per thread
// #define MAX_ENTRIES_PER_THREAD 20000
// #define MAX_ENTRIES_PER_THREAD 400000
#define MAX_ENTRIES_PER_THREAD 800000
uint64_t* intervals;

uint64_t* init_intervals_arr() {
	intervals = (uint64_t*)malloc(sizeof(uint64_t) * num_threads * MAX_ENTRIES_PER_THREAD);
	if (!intervals) {
		ERROR("Failed to allocate intervals array");
	}
	memset(intervals, 0, sizeof(uint64_t) * num_threads * MAX_ENTRIES_PER_THREAD);
	return intervals;
}

inline void record_interval(int tid, int idx, uint64_t val) {
	intervals[MAX_ENTRIES_PER_THREAD * (tid-THREAD_OFFSET) + idx] = val;
}

void dump_intervals() {
	FILE *fp = fopen("intervals.data", "wb");
	// fwrite(intervals, sizeof(uint64_t), sizeof(intervals), f);
	// fclose(f);
	fprintf(fp, "num_threads %lu\n", num_threads);

	if (TIMING_METHOD == 0) {
		fprintf(fp, "timing_method rdtsc\n");
	}
	else {
		fprintf(fp, "timing_method gettimeofday\n");
	}


	fprintf(fp, "max_records_per_thread %d\n", MAX_ENTRIES_PER_THREAD);
	fprintf(fp, "num_records_per_thread\n");
	for (int i = 0; i < num_threads; ++i) {
		fprintf(fp, "%d %lu\n", i, num_interrupts[i+2]);
	}

	fprintf(fp, "data\n");
	for (int i = 0; i < num_threads * MAX_ENTRIES_PER_THREAD; ++i) {
		fprintf(fp, "%lu\n", intervals[i]);
	}
}
// END code for recording individual intervals per thread


static inline int get_thread_id() {
  return syscall(SYS_gettid);
}

//used in DEBUG statements
uint64_t find_my_thread() {
  pthread_t me = pthread_self();
  uint64_t i;

  if (!tid) {
    return 1;
  }

  for (i = 0; i < (num_threads + THREAD_OFFSET); i++) {
    if (me == tid[i]) {
      return i;
    }
  }
  return -1;
}


static inline uint64_t __attribute__((always_inline))
rdtsc (void)
{
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}

static inline uint64_t measure_time() {
  if (!TIMING_METHOD) {
    uint64_t temp = rdtsc();
    return temp;
  } else {
  	// TODO Need to put gettimeofday here instead of clock_gettime
  	// can also replace CLOCK_MONTONIC with CLOCK_REALTIME?
  	// see https://stackoverflow.com/a/38916054/6668130
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000 + t.tv_nsec / 1000;
  }
}


static void reset_timer(uint64_t which) {
  struct itimerspec it; 

  it.it_interval.tv_sec  = 0;
  it.it_interval.tv_nsec = 0;
  it.it_value.tv_sec     = 0;
  it.it_value.tv_nsec    = interrupt_us * 1000;

  //DEBUG("%lu setting repeating timer interrupt for %lu us from now\n", which, interrupt_us);
  last[which] = measure_time();
  if (timer_settime(timer[which], 0, &it, 0)) {
    ERROR("Failed to set timer?!\n");
  }
}


static void* make_item(uint64_t which, uint64_t tag, uint64_t isintr) {
  uint64_t gen = (isintr << 63) + (which << 32) + tag;
  return (void*)gen;
}
//
// note that printing is dangerous here since it's in signal context
//
static void handler(int sig, siginfo_t* si, void* priv) {
  
  uint64_t cur = measure_time();
  uint64_t which = si->si_value.sival_int;
  
  uint64_t interval = cur - last[which];
  
  if (interval ==0) {
    DEBUG("INTERVAL WAS 0!!!!!!");
  }
  
  record_interval(which, num_interrupts[which], interval);
  num_interrupts[which] += 1;

  reset_timer(which); 
  last[which] = cur;
}

static void thread_work(uint64_t which) {
  DEBUG("thread %lu\n", which);

  
  uint64_t i;
  volatile uint64_t gen = 0;
  // wait for all producers and consumers to be ready
  pthread_barrier_wait(&barrier);
  
  DEBUG("thread %lu about to start timer\n", which);
  // now schedule a timer interrupt for ourselves
  
  reset_timer(which);
  DEBUG("%lu started timer\n", which);
  

  for (i = 0; i < AMT_WORK; i++) { //this is the fake work loop
    gen = (uint64_t)make_item(which, rand(), 1);
  }
  //DEBUG("\t%lu done with work\n", which);
  

  // turn off further producer interrupts before we add terminal item
  signal(INTERRUPT_SIGNAL, SIG_IGN);
    

  // wait again for everyone
  pthread_barrier_wait(&barrier);
  
  

  return;

}
static void print_arrays() {
  uint64_t i; 
  
  for (i=0; i< (num_threads + THREAD_OFFSET); i++) {
    DEBUG("num_interrupts %lu : %lu \n", i, num_interrupts[i]);
    if (num_interrupts[i] > MAX_ENTRIES_PER_THREAD) {
    	DEBUG("WARNING, TOO MANY ENTRIES!\n");
    	DEBUG("RUN WITH HIGHER VALUE FOR NUM_ENTRIES_PER_THREAD\n");
    }
  }

  



}
void* worker(void* arg) {
  long myid  = (long)arg;
  long mytid = tid[myid];

  DEBUG("-Hello from thread %ld (tid %ld)\n", myid, mytid);
  uint64_t cpu;
/* Keeping this part for reference
  if (myid < num_producers) {
    if (static_mapping_producers) {
      cpu = myid % num_cpus_producers;
    } else {
      cpu = -1ULL;
    }
  }
  if (myid >= num_producers) {
    if (static_mapping_consumers) {
      if (!static_mapping_producers) {
        cpu = myid % num_cpus_consumers;
      } else {
        cpu = num_cpus_producers + myid % num_cpus_consumers;
      }
    } else {
      cpu = -1ULL;
    }
  }
*/
  cpu = myid % num_cpus; // should end up being one thread to each cpu though

  if (cpu == -1ULL) {
    DEBUG("Thread is not assigned to any specific cpu\n");
  } else {

    DEBUG("Thread is assigned to cpu %lu\n", cpu);

    // put ourselves on the desired processor
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    
    int affinity_res = sched_setaffinity(0, sizeof(set), &set);
    if (affinity_res < 0) { // do it
      ERROR("Can't setaffinity in thread, %d\n", affinity_res);
      exit(-1);
    }
  }

  // build timer for the thread here
  // the timer will actually be set in the producer or consumer function
  if (interrupts) {
    struct sigevent sev; //so this generates the signal that the code in main then responds to?

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify          = SIGEV_THREAD_ID;
    sev.sigev_signo           = INTERRUPT_SIGNAL;
    sev._sigev_un._tid        = get_thread_id();
    sev.sigev_value.sival_int = myid;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timer[myid])) {
      ERROR("Failed to create timer\n");
      exit(-1);
    }
  }
  thread_work(myid);
  DEBUG("Thread done and now exiting\n");

  pthread_exit(NULL); // finish - no return value

}


void usage() {
  fprintf(stderr, "usage: harness \n");
  
}



int main(int argc, char* argv[]) {
  uint64_t i;
  long rc;
  
  num_cpus = get_nprocs_conf();  
  num_threads = num_cpus;


  srand(time(NULL));
  
  
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));

  sa.sa_sigaction = handler; //Sets interrupt handler!
  sa.sa_flags    |= SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, INTERRUPT_SIGNAL); // do not interrupt our own interrupt handler
  if (sigaction(INTERRUPT_SIGNAL, &sa, 0)) {
    ERROR("Failed to set up signal handler\n");
    return -1;
  }

  // allocate timer array
  timer = (timer_t*)malloc(sizeof(timer_t) * (num_threads));
  if (!timer) {
    ERROR("Failed to allocate timer array\n");
    return -1;
  }
  memset(timer, 0, sizeof(timer_t) * (num_threads));
  DEBUG("Allocated array of %ld timers\n", num_threads);



  

  last = (uint64_t*)malloc(sizeof(uint64_t) * (num_threads + THREAD_OFFSET)); //array of last interrupt time
  memset(last, 0, sizeof(uint64_t) * (num_threads + THREAD_OFFSET));

  num_interrupts = (uint64_t*)malloc(sizeof(uint64_t) * (num_threads + THREAD_OFFSET)); //array of interrupt count for each thread
  memset(num_interrupts, 0, sizeof(uint64_t) * (num_threads + THREAD_OFFSET));


 
  tid = (pthread_t*)malloc(sizeof(pthread_t) * (num_threads + THREAD_OFFSET));

  if (!tid) {
    ERROR("Cannot allocate tids\n");
    return -1;
  }

  intervals = init_intervals_arr();



  memset(tid, 0, sizeof(sizeof(pthread_t) * (num_threads + THREAD_OFFSET)));

  tid[0] = -1;
  tid[1] = pthread_self();

  if (pthread_barrier_init(&barrier, 0, num_threads)) {
    ERROR("Failed to allocate barrier\n");
    return -1;
  }

  INFO("Starting %lu software threads\n", num_threads);

  for (i = THREAD_OFFSET; i < (num_threads + THREAD_OFFSET); i++) {
    rc = pthread_create(&(tid[i]), // thread id gets put here
                        NULL,      // use default attributes
                        worker,    // thread will begin in this function
                        (void*)i   // we'll give it i as the argument
                        );
    if (rc == 0) {
      DEBUG("Started thread %lu, tid %lu\n", i, tid[i]);
    } else {
      ERROR("Failed to start thread %lu\n", i);
      return -1;
    }
  }

  DEBUG("Finished starting threads\n");

  DEBUG("Now joining\n");
  

  for (i = THREAD_OFFSET; i < (num_threads + THREAD_OFFSET); i++) {
    DEBUG("Joining with %lu, tid %lu\n", i, tid[i]);
    rc = pthread_join(tid[i], NULL);
    if (rc != 0) {
      ERROR("Failed to join with %lu!\n", i);
      return -1;
    } else {
      DEBUG("Done joining with %lu\n", i);
    }
  }
  print_arrays();
  

  pthread_barrier_destroy(&barrier);

  free(tid);

  INFO("Done!\n");

  dump_intervals(intervals);
  free(intervals);

  return 0;
}

