/*
 *
 *  Producer-Consumer Lab
 *
 *  Copyright (c) 2019 Peter A. Dinda
 *
 *  Warning: This file will be overwritten before grading.
 */

#ifndef _CONFIG
#define _CONFIG

#define DEBUG_OUTPUT        0 // have DEBUG()s print something
#define OUTPUT_SHOWS_THREAD 1 // show thread number in debugging output

// Used to to fake interrupts using itimers, one per thread
#define INTERRUPT_TIMER  ITIMER_REAL    // use realtime timer
#define INTERRUPT_SIGNAL SIGALRM        // what itimer sends on expiration


uint64_t find_my_thread();


#if DEBUG_OUTPUT
  #if OUTPUT_SHOWS_THREAD
    #define DEBUG(fmt, args...) fprintf(stderr, "(%d) %s(%d): debug: " fmt, (int)find_my_thread(), __FILE__, __LINE__, ##args)
  #else
    #define DEBUG(fmt, args...) fprintf(stderr, "%s(%d): debug: " fmt, __FILE__, __LINE__, ##args)
  #endif
#else
  #define DEBUG(fmt, args...) // nothing
#endif

#if OUTPUT_SHOWS_THREAD
  #define ERROR(fmt, args...) fprintf(stderr, "(%d) %s(%d): ERROR: " fmt, (int)find_my_thread(), __FILE__, __LINE__, ##args)
  #define INFO(fmt, args...)  fprintf(stderr, "(%d) %s(%d): " fmt, (int)find_my_thread(), __FILE__, __LINE__, ##args)
#else
  #define ERROR(fmt, args...) fprintf(stderr, "%s(%d): ERROR: " fmt, __FILE__, __LINE__, ##args)
  #define INFO(fmt, args...)  fprintf(stderr, "%s(%d): " fmt, __FILE__, __LINE__, ##args)
#endif


#endif

