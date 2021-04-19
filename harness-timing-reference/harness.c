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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <math.h>

#include "config.h"
#include "ring.h"
#include "atomics.h"


struct timeval start;
struct timeval end;


#define EXTERMINATE -1ULL

pthread_barrier_t barrier;

int static_mapping_producers;
int static_mapping_consumers;
uint64_t seed_value = -1ULL;
uint64_t num_threads;
uint64_t num_cpus_producers;
uint64_t num_cpus_consumers;
uint64_t num_producers;
uint64_t num_consumers;
uint64_t num_ops;
int interrupt_producers;
int interrupt_consumers;
uint64_t interrupt_mean_arrival_us = 10000; // 10 ms by default


// in order to match up with gdb:
// tid[0] => nothing
// tid[1] => main
// tid[2] => 1st producer thread
#define THREAD_OFFSET 2
pthread_t* tid;   // array of thread ids

// array of timer ids, if used - timer[0] => first interrupt producer
// we need to use timer_create, etc to actually do per-thread timers
timer_t* timer;

ring_t* ring;

uint64_t producer_checksum;
uint64_t producer_count;
uint64_t producer_interrupt_count;
uint64_t producer_interrupt_successes;
uint64_t consumer_checksum;
uint64_t consumer_count;
uint64_t consumer_interrupt_count;
uint64_t consumer_interrupt_successes;



static inline int get_thread_id() {
  return syscall(SYS_gettid);
}

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


// Generate exponentially distributed random number with the given mean
// result is in us for use with itimer
static uint64_t exp_rand(uint64_t mean_us) {
  uint64_t ret = 0;

  double u = drand48();

  // u = [0,1), uniform random, now convert to exponential

  u = -log(1.0 - u) * ((double)mean_us);

  // now shape u back into a uint64_t and return

  if (u > ((double)(-1ULL))) {
    ret = -1ULL;
  } else {
    ret = (uint64_t)u;
  }

  // corner case
  if (ret == 0) {
    ret = 1;
  }

  return ret;
}



static void reset_timer(uint64_t which) {
  struct itimerspec it;

  uint64_t n = exp_rand(interrupt_mean_arrival_us);

  it.it_interval.tv_sec  = 0;
  it.it_interval.tv_nsec = 0;
  it.it_value.tv_sec     = n / 1000000;
  it.it_value.tv_nsec    = (n % 1000000) * 1000;

  // DEBUG("setting next timer interrupt for %lu us from now\n", n);

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
static void interrupt_producer(uint64_t which) {
  int rc;

  // DEBUG("timer interrupt - trying to produce item\n");
  atomic_fetch_and_add(&producer_interrupt_count, 1);

  uint64_t gen = (uint64_t)make_item(which, rand(), 1);
  rc = ring_try_push(ring, (void*)gen);
  if (rc < 0) {
    ERROR("interrupt producer %lu failed to push item %016lx...\n", which, gen);
  } else if (rc == 0) {
    // DEBUG("interrupt producer %lu pushed %016lx\n", which, gen);
    atomic_fetch_and_add(&producer_checksum, gen);
    atomic_fetch_and_add(&producer_count, 1);
    atomic_fetch_and_add(&producer_interrupt_successes, 1);
  } else { // rc>0
    // DEBUG("interrupt producer %lu encounters full ring on pushing item %016lx\n", which, gen);
  }

  reset_timer(which);
}

//
// note that printing is dangerous here since it's in signal context
//
static void interrupt_consumer(uint64_t which) {
  int rc;
  uint64_t gen;

  // DEBUG("timer interrupt - trying to consume item\n");

  atomic_fetch_and_add(&consumer_interrupt_count, 1);

  rc = ring_try_pull(ring, (void**)&gen);

  if (rc < 0) {
    ERROR("interrupt consumer %lu failed to pull item\n", which);
    reset_timer(which);
  } else if (rc == 0) {
    // DEBUG("interrupt consumer %lu pulled item %016lx\n", which, gen);
    if (gen == EXTERMINATE) {
      // we do not want to handle the stopping condition here
      // in the interrupt handler, other than to stop further interrupts
      // repush terminate so that a thread can see it.
      do {
        rc = ring_try_push(ring, (void*)gen);
      } while (rc > 0);
      if (rc < 0) {
        ERROR("interrupt consumer %lu failed to repush terminate\n", which);
      } else {
        // DEBUG("interrupt consumer %lu repushed terminate\n", which);
      }
      // do not set timer,
      // do not consume
    } else {
      // normal dequeue
      atomic_fetch_and_add(&consumer_checksum, gen);
      atomic_fetch_and_add(&consumer_count, 1);
      atomic_fetch_and_add(&consumer_interrupt_successes, 1);
      reset_timer(which);
    }
  } else { // rc>0
    // DEBUG("interrupt consumer %lu encounters empty ring on pulling item\n",which);
    reset_timer(which);
  }
}


static void interrupt_trampoline(int sig, siginfo_t* si, void* priv) {
  uint64_t which = si->si_value.sival_int;

  if (which < num_producers) {
    interrupt_producer(which);
  } else {
    interrupt_consumer(which);
  }
}



static void thread_producer(uint64_t which) {
  DEBUG("producer %lu\n", which);

  uint64_t mypart = (num_ops / num_producers) + (which < (num_ops % num_producers));
  uint64_t mypartterminals = (num_consumers / num_producers) + (which < (num_consumers % num_producers));
  uint64_t i;
  uint64_t local_checksum = 0;
  uint64_t local_count    = 0;

  // wait for all producers and consumers to be ready
  pthread_barrier_wait(&barrier);

  if (which == 0) { // first producer does timing
    gettimeofday(&start, 0);
  }

  // now schedule a timer interrupt for ourselves
  if (interrupt_producers) {
    reset_timer(which);
  }

  for (i = 0; i < mypart; i++) {
    uint64_t gen = (uint64_t)make_item(which, i, 0);
    DEBUG("producer %lu will try to push item %016lx\n", which, gen);
    if (ring_push(ring, (void*)gen)) {
      ERROR("producer %lu failed to push item %016lx...\n", which, gen);
    } else {
      DEBUG("producer %lu pushed %016lx\n", which, gen);
    }
    local_checksum += gen;
    local_count++;
  }

  // turn off further producer interrupts before we add terminal item
  if (interrupt_producers) {
    signal(INTERRUPT_SIGNAL, SIG_IGN);
  }

  // precompute effects of pushing the terminal values
  for (i = 0; i < mypartterminals; i++) {
    local_checksum += EXTERMINATE;
    local_count++;
  }

  atomic_fetch_and_add(&producer_checksum, local_checksum);
  atomic_fetch_and_add(&producer_count, local_count);

  if ((num_producers > num_consumers) && mypartterminals) {
    // in this case, we might place terminals before some other producer
    // is actually done, which could cause the consumers to quit early
    // so instead we will wait until all producers are done
    while (atomic_load(&producer_count) < num_ops) {
      pthread_yield();
    }
  }

  // actually push the terminals
  for (i = 0; i < mypartterminals; i++) {
    if (ring_push(ring, (void*)EXTERMINATE)) { // Dalek-style
      ERROR("producer %lu failed to push terminal item\n", which);
    } else {
      DEBUG("producer %lu pushed terminal item\n", which);
    }
  }

  // wait again for everyone
  pthread_barrier_wait(&barrier);

  if (which == 0) { // first producer does timing
    gettimeofday(&end, 0);
  }

  return;

}

void thread_consumer(uint64_t which) {
  DEBUG("consumer %lu\n", which);

  // wait for all producers and consumers to be ready
  pthread_barrier_wait(&barrier);

  // now schedule a timer interrupt for ourselves
  if (interrupt_consumers) {
    reset_timer(num_producers + which);
  }

  uint64_t gen;
  uint64_t local_checksum = 0;
  uint64_t local_count    = 0;

  do {
    DEBUG("consumer %lu will try to pull item\n", which);
    if (ring_pull(ring, (void**)&gen)) {
      ERROR("consumer %lu failed to pull item\n", which);
    } else {
      DEBUG("consumer %lu pulled item %016lx\n", which, gen);
    }
    local_checksum += gen;
    local_count++;
  } while (gen != EXTERMINATE);

  DEBUG("consumer %lu pulled terminal item\n", which);

  // turn off further consumer interrupts
  if (interrupt_consumers) {
    signal(INTERRUPT_SIGNAL, SIG_IGN);
  }

  atomic_fetch_and_add(&consumer_checksum, local_checksum);
  atomic_fetch_and_add(&consumer_count, local_count);

  pthread_barrier_wait(&barrier);

  return;

}



void* worker(void* arg) {
  long myid  = (long)arg;
  long mytid = tid[myid];

  DEBUG("Hello from thread %ld (tid %ld)\n", myid, mytid);

  myid -= THREAD_OFFSET; // eliminate offset so we can compute producer 0.., instead of 2..

  uint64_t cpu;

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

  if (cpu == -1ULL) {
    DEBUG("Thread is not assigned to any specific cpu\n");
  } else {

    DEBUG("Thread is assigned to cpu %lu\n", cpu);

    // put ourselves on the desired processor
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) < 0) { // do it
      ERROR("Can't setaffinity in thread\n");
      exit(-1);
    }
  }

  // build timer for the thread here
  // the timer will actually be set in the producer or consumer function
  if (interrupt_producers || interrupt_consumers) {
    struct sigevent sev;

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify          = SIGEV_THREAD_ID;
    sev.sigev_signo           = INTERRUPT_SIGNAL;
    sev._sigev_un._tid        = get_thread_id();
    sev.sigev_value.sival_int = myid;

    if (timer_create(CLOCK_REALTIME, &sev, &timer[myid])) {
      ERROR("Failed to create timer\n");
      exit(-1);
    }
  }

  if (myid < num_producers) {
    thread_producer(myid);
  } else {
    thread_consumer(myid - num_producers);
  }

  DEBUG("Thread done and now exiting\n");

  pthread_exit(NULL); // finish - no return value

}


void usage() {
  fprintf(stderr, "usage: harness [-i pc] [-t a] [-p x] [-c y] [-s z] [-h] nump numc size numo\n");
  fprintf(stderr, "  Execute a producer-consumer system\n");
  fprintf(stderr, "    nump  = number of producer threads\n");
  fprintf(stderr, "    numc  = number of consumer threads\n");
  fprintf(stderr, "    size  = number of slots in queue/ring\n");
  fprintf(stderr, "    numo  = total number of produce/consume calls\n");
  fprintf(stderr, "           (they are roughly evenly spread across threads)\n");
  fprintf(stderr, "    -i pc = produce (p) and/or consume (c) from interrupts\n");
  fprintf(stderr, "            in addition to threads\n");
  fprintf(stderr, "    -t a  = interrupts have poisson arrivals with a mean of a (us)\n");
  fprintf(stderr, "    -p x  = statically map producers across x processors\n");
  fprintf(stderr, "    -c y  = statically map consumers across y processors\n");
  fprintf(stderr, "    -s z  = use z as the seed value\n");
  fprintf(stderr, "    -h    = help (this message)\n");
}



int main(int argc, char* argv[]) {
  uint64_t i;
  long rc;
  uint32_t ring_size;
  int opt;

  while ((opt = getopt(argc, argv, "i:t:p:c:s:h")) != -1) {
    switch (opt) {
      case 'h':
        usage();
        return 0;
        break;
      case 'i':
        if (strstr(optarg, "p")) {
          interrupt_producers = 1;
        }
        if (strstr(optarg, "c")) {
          interrupt_consumers = 1;
        }
        break;
      case 't':
        interrupt_mean_arrival_us = atol(optarg);
        break;
      case 'p':
        static_mapping_producers = 1;
        num_cpus_producers       = atol(optarg);
        break;
      case 'c':
        static_mapping_consumers = 1;
        num_cpus_consumers       = atol(optarg);
        break;
      case 's':
        seed_value = atol(optarg);
        break;
      default:
        ERROR("Unknown option %c\n", opt);
        return -1;
        break;
    }
  }

  if ((argc - optind) != 4) {
    usage();
    return -1;
  }

  num_producers = atol(argv[optind]);
  num_consumers = atol(argv[optind + 1]);
  num_threads   = num_producers + num_consumers;
  ring_size     = atoi(argv[optind + 2]);
  num_ops       = atol(argv[optind + 3]);

  if (seed_value != -1ULL) {
    srand(seed_value);
  } else {
    srand(time(NULL));
  }

  if (interrupt_producers || interrupt_consumers) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = interrupt_trampoline;
    sa.sa_flags    |= SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, INTERRUPT_SIGNAL); // do not interrupt our own interrupt handler
    if (sigaction(INTERRUPT_SIGNAL, &sa, 0)) {
      ERROR("Failed to set up signal handler\n");
      return -1;
    }

    timer = (timer_t*)malloc(sizeof(timer_t) * (num_threads));
    if (!timer) {
      ERROR("Failed to allocate timer array\n");
      return -1;
    }
    memset(timer, 0, sizeof(timer_t) * (num_threads));

  }

  ring = ring_create(ring_size,
                     num_producers,
                     interrupt_producers,
                     num_consumers,
                     interrupt_consumers);

  if (!ring) {
    ERROR("Failed to create ring\n");
    return -1;
  }

  tid = (pthread_t*)malloc(sizeof(pthread_t) * (num_threads + THREAD_OFFSET));

  if (!tid) {
    ERROR("Cannot allocate tids\n");
    return -1;
  }

  memset(tid, 0, sizeof(sizeof(pthread_t) * (num_threads + THREAD_OFFSET)));

  tid[0] = -1;
  tid[1] = pthread_self();

  if (pthread_barrier_init(&barrier, 0, num_threads)) {
    ERROR("Failed to allocate barrier\n");
    return -1;
  }

  INFO("Starting %lu software threads (%lu producers, %lu consumers)\n", num_threads, num_producers, num_consumers);

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

  DEBUG("Producer checksum: %016lx\n", producer_checksum);
  DEBUG("Consumer checksum: %016lx\n", consumer_checksum);

  if (producer_checksum != consumer_checksum) {
    printf("INCORRECT:  producer and consumer checksums do not match\n");
  } else {
    printf("Possibly Correct:  producer and consumer checksums match\n");
  }

  if (producer_count != consumer_count) {
    printf("INCORRECT:  producer and consumer counts do not match\n");
  } else {
    printf("Possibly Correct:  producer and consumer counts match\n");
  }

  double dur;

  dur = end.tv_sec - start.tv_sec;

  if (end.tv_usec >= start.tv_usec) {
    dur += (end.tv_usec - start.tv_usec) / 1000000.0;
  } else {
    dur += (1000000 + (end.tv_usec - start.tv_usec)) / 1000000.0;
  }

  printf("Producers:                     %lu\n", num_producers);
  if (interrupt_producers) {
    printf("Producer interrupts:           %lu\n", producer_interrupt_count);
    printf("Producer interrupt successes:  %lu\n", producer_interrupt_successes);
  }
  printf("Consumers:                     %lu\n", num_consumers);
  if (interrupt_consumers) {
    printf("Consumer interrupts:           %lu\n", consumer_interrupt_count);
    printf("Consumer interrupt successes:  %lu\n", consumer_interrupt_successes);
  }
  printf("Duration:                      %lf seconds\n", dur);
  printf("Consumed:                      %lu elements\n\n", consumer_count);
  printf("Throughput:                    %lf elements/s\n", consumer_count / dur);

  pthread_barrier_destroy(&barrier);

  free(tid);

  ring_destroy(ring);

  INFO("Done!\n");

  return 0;
}

