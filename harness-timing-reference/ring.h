/*
 *
 *  Producer-Consumer Lab
 *
 *  Copyright (c) 2019 Peter A. Dinda
 *
 */

#ifndef _RING_H
#define _RING_H

#include <stdint.h>

// For performance, this should be a multiple of the cache line size
#define CACHE_ALIGN_SIZE 64
#define CACHE_ALIGN __attribute__((aligned(CACHE_ALIGN_SIZE)))

// structure definition is here to make it possible to optimize via inlining
// push/pull, etc
typedef volatile int lock_t;

typedef struct _ring {
  uint64_t size;     // number of elements on queue


  uint64_t head;     // where next push goes
  uint64_t tail;     // where last pull came from
  int can_interrupt;
  //
  // WRITE ME!
  //
  // synchronization variables (e.g. locks) would go here
  // note that you should think carefully about their alignment
  int oldstate;
  // the ring consists of size elements
  // each element is a void pointer and thus we
  // can enqueue anything by reference
  // the "[0]" here is a C idiom that allows us to
  // have trailing data
  void* elements[0]  CACHE_ALIGN;

} CACHE_ALIGN ring_t;


ring_t* ring_create(uint32_t size,            // how many things can be in the ring at once
                    uint32_t producer_limit,  // limit, if any, on number of producers (0=none)
                    int producer_interrupts,  // true if producers can run in interrupt handlers
                    uint32_t consumer_limit,  // limit, if any, on the number of consumers (0=none)
                    int consumer_interrupts); // true if consumers can run in interrupt handlers

void ring_destroy(ring_t* ring);


// Producer Side

// push data into the ring, blocking until possible
// returns 0 on success, negative on error
int ring_push(ring_t* ring, void* data);

// push data into the ring, if possible
// returns 0 on success, negative on error, 1 if the ring is full
int ring_try_push(ring_t* ring, void* data);
void lock(volatile lock_t *lock);
void unlock(volatile lock_t *lock);

// Consumer Side

// pull data from the ring
// returns 0 on success, negative on error
// data is returned via reference (2nd parameter)
int ring_pull(ring_t* ring, void** data);

// pull data from the ring, if possible
// returns 0 on success, negative on error, 1 if the ring is empty
// data is returned via reference (2nd parameter)
int ring_try_pull(ring_t* ring, void** data);


#endif

