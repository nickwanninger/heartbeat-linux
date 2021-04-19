/*
 *
 *  Producer-Consumer Lab
 *
 *  Copyright (c) 2019 Peter A. Dinda
 *
 */

#ifndef _ATOMICS_
#define _ATOMICS_


//
// AVERT YOUR EYES - THIS HIDEOUS CRUD IS FOR COMPATABILITY WITH ANCIENT COMPILERS
//
#if __GNUC__ < 5
#define __ATOMIC_SEQ_CST 0
#define __atomic_load_n(srcptr,   cmodel)         __sync_fetch_and_or(srcptr, 0)
#define __atomic_store_n(destptr, value, cmodel)  ({ *(destptr) = value; __sync_synchronize(); })
#endif
//
// IT IS NOW SAFE TO LOOK
//


// atomic load from pointer with sequential consistency
#define atomic_load(srcptr)         __atomic_load_n(srcptr, __ATOMIC_SEQ_CST)

// atomic store to pointer with sequenial consistency
#define atomic_store(destptr, value) __atomic_store_n(destptr, value, __ATOMIC_SEQ_CST)

// atomically add incr to *destptr and return its previous value
#define atomic_fetch_and_add(destptr, incr) __sync_fetch_and_add(destptr, incr)

// atomically compare oldval to *destptr
// if equal, set *destptr to newval, and return true
// if not equal, return false
#define atomic_compare_and_swap(destptr, oldval, newval) __sync_bool_compare_and_swap(destptr, oldval, newval)

// atomically read the old value at destptr, and write it with
// newval.  Return the old value
//
#define atomic_test_and_set(destptr, newval) __sync_lock_test_and_set(destptr, newval)

// atomically read the old value at destptr, and write it with
// newval.  Return the old value
//
#define atomic_exchange(destptr, newval) __atomic_exchange_n(destptr, newval, __ATOMIC_SEQ_CST)

//
// Tell cpu it can throttle us down
//
#define pause() __asm__ __volatile__ ("pause" : : :)

// disable interrupts of this thread, and return, via oldstate
// the previous state of interrupts (enabled/disabled)
// this is like an irq_disable_save() in a kernel
void interrupt_disable_save(int* oldstate);

// restores state of interrupts on this thread to that of the argument
// this is like an irq_enable_restore() in a kernel
void interrupt_enable_restore(int oldstate);

//
// Force the compiler to emit all loads+stores from code prior to this
// call before any load or store from code subsequent to this call.
// In other words, the compiler cannot move loads or stores across this
// boundary during optimization
#define software_memory_barrier() __asm__ __volatile__ ("" : : : "memory")



//
// Force the hardware to complete any loads or stores from instructions
// prior to this call before it can start any loads or stores from instructions
// subsequent to the call.  After this call, other cpus should be aware of
// all writes made by this cpu prior to the call.
//
// This also a software memory barrier
//
#define hardware_memory_barrier() __sync_synchronize()


//
// Define your synchronization primitive's types and perhaps even functions here
//
// WRITE ME!
typedef volatile int lock_t;
void* my_queue[0]; 


//
//


#endif

