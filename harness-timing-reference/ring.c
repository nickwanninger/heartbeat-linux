/*
 *
 *  Producer-Consumer Lab
 *
 *  Copyright (c) 2019 Peter A. Dinda
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "atomics.h"
#include "ring.h"



ring_t* ring_create(uint32_t size,
                    uint32_t producer_limit,
                    int producer_interrupts,
                    uint32_t consumer_limit,
                    int consumer_interrupts) {
  // the handout version does not observe the limits or interrupts

  uint64_t numbytes = sizeof(ring_t) + (size * sizeof(void*));
  ring_t* ring      = (ring_t*)malloc(numbytes);

  if (!ring) {
    ERROR("Cannot allocate\n");
    return 0;
  }

  memset(ring, 0, numbytes);

  DEBUG("allocation is at %p, data at %p\n", ring, ring->elements);

  ring->size = size;
  ring->head = 0;
  ring->tail = 0;
  ring->can_interrupt = producer_interrupts || consumer_interrupts;
  
  DEBUG("ring %p created\n", ring);

  // WRITE ME!
  //
  // any synchronization state would be initialized here
  //int oldstate = 1;
  //ring->oldstate = 1;

  return ring;
}

void ring_destroy(ring_t* ring) {
  DEBUG("destroy %p\n", ring);
  // do we need to wait for requests to finish?
  free(ring);
}
int t_state;

// Helper Functions

// note that this is NOT synchronized and so prone to races
static int can_push(ring_t* ring) {
  return (ring->head - ring->tail) < ring->size;
}

// note that this is NOT synchronized and so prone to races!
static int can_pull(ring_t* ring) {
  return ring->tail < ring->head;
}


// Producer Side

inline void lock(volatile lock_t *lock){
	while(atomic_test_and_set(lock, 1)) {
	//interrupt_enable_restore(ring->oldstate);
	}
}
void unlock(volatile lock_t *lock) {
	*lock = 0;
}

lock_t counter_lock = 0;

// note that this is NOT synchronized and so prone to races!
int ring_push(ring_t* ring, void* data) {
  int int_state;
  //interrupts_off();
  // WRITE ME!
  //
  // THIS IS WRONG IN THE PRESENCE OF CONCURRENCY
  // YOU NEED TO FIX IT

  DEBUG("starting push of %p to %p\n", data, ring);
  if (ring->can_interrupt) {
  	interrupt_disable_save(&int_state);
  }
  while(1) {
  
  lock(&counter_lock);
  //interrupt_enable_restore(oldstate);
 
  if ((ring->head - ring->tail) >= ring->size) { 
	unlock(&counter_lock);
	
	//interrupt_enable_restore(int_state);
  } else {
  ring->elements[ring->head % ring->size] = data;
  ring->head++;
  unlock(&counter_lock);
  if (ring->can_interrupt) {
  interrupt_enable_restore(int_state);
  }
  //interrupt_enable_restore(int_state);
  DEBUG("push done\n");

  return 0;
  }
  }
 
}

// note that this is NOT synchronized and so prone to races!
int ring_try_push(ring_t* ring, void* data) {
  int int_state;
  // WRITE ME!
  //
  // THIS IS WRONG IN THE PRESENCE OF CONCURRENCY
  // YOU NEED TO FIX IT
  //interrupt_disable_save(&oldstate);
  //interrupt_enable_restore(ring->oldstate);
  
  //interrupt_enable_restore(ring->oldstate);
  if (ring->can_interrupt) {
  interrupt_disable_save(&int_state);
  }
  //interrupt_enable_restore(int_state);
  DEBUG("TRYPUSHCALLED");
  //unlock(&counter_lock);
  lock(&counter_lock);
  if (!can_push(ring)) {
    unlock(&counter_lock);
    if (ring->can_interrupt) {
    interrupt_enable_restore(int_state);
    }
    return 1;
  }
 
  ring->elements[ring->head % ring->size] = data;
  ring->head++;
  unlock(&counter_lock);
  if (ring->can_interrupt) {
  interrupt_enable_restore(int_state);
  }
  return 0;
}


// Consumer Side

// note that this is NOT synchronized and so prone to races!
int ring_pull(ring_t* ring, void** data) {
  // WRITE ME!
  //
  // THIS IS WRONG IN THE PRESENCE OF CONCURRENCY
  // YOU NEED TO FIX IT
  int int_state;
  //interrupt_disable_save(&int_state);
  //interrupt_enable_restore(int_state);
  //interrupt_enable_restore(ring->oldstate);
  //interrupt_disable_save(&(ring->oldstate));
  //interrupt_enable_restore(ring->oldstate);
  DEBUG("starting pull from %p\n", ring);
  DEBUG("%ld", ring->head);
  DEBUG("%ld", ring->tail);
  DEBUG("%d", counter_lock);
  while(1) {
  //interrupt_enable_restore(ring->oldstate);
  //interrupt_disable_save(&(ring->oldstate));
  if (ring->can_interrupt) {
  interrupt_disable_save(&int_state);
  }
  lock(&counter_lock);
   if ( ring->tail >= ring->head) { 
	unlock(&counter_lock);
	
	//interrupt_enable_restore(ring->oldstate);	 
   } else {
   	*data = ring->elements[ring->tail % ring->size];
  	ring->tail++;
  	unlock(&counter_lock);
  	if (ring->can_interrupt) {
  	interrupt_enable_restore(int_state);
  	}
  	//interrupt_enable_restore(ring->oldstate);
  	//interrupt_enable_restore(int_state);
  	DEBUG("pulled %p\n", *data);
  	return 0;
  }
  }
}

// note that this is NOT synchronized and so prone to races!
int ring_try_pull(ring_t* ring, void** data) {
  // WRITE ME!
  //
  // THIS IS WRONG IN THE PRESENCE OF CONCURRENCY
  // YOU NEED TO FIX IT
  //interrupt_disable_save(&oldstate);
  //interrupt_enable_restore(ring->oldstate);
  int int_state;
  //interrupt_enable_restore(ring->oldstate);
  if (ring->can_interrupt) {
  interrupt_disable_save(&int_state);
  }
  lock(&counter_lock);
  if (!can_pull(ring)) {
    unlock(&counter_lock);
    if (ring->can_interrupt) {
    interrupt_enable_restore(int_state);
    }
    return 1;
  }
  *data = ring->elements[ring->tail % ring->size];
  ring->tail++;
  unlock(&counter_lock);
  if (ring->can_interrupt) {
  interrupt_enable_restore(int_state);
  }
  return 0;
}

