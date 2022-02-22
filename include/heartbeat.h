#pragma once

// This header defines the interface by which a userspace process can
// request a heartbeat interval


#include "heartbeat_kmod.h"
#include <stdint.h>



typedef struct {
  uint64_t rax;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rdx;
  uint64_t r10;
  uint64_t r8;
  uint64_t r9;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rbp;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
	uint64_t rflags;
  uint64_t rip;
} hb_regs_t;


typedef void (*hb_callback_t)(hb_regs_t *);

// initialize heartbeat for this thread.
// There exists an invariant that the thread must be locked
// to a certain CPU core because hrtimer only delivers to the core
// it is created on. Also, we want to avoid all kinds of IPI and
// signals because they have been shown to be slow.
int hb_init(int cpu);

// Exit the heartbeat timer, destroying the timer in the kernel
void hb_exit();

// schedule a heartbeat to occur once in `us` microseconds
int hb_oneshot(uint64_t us, hb_callback_t callback);
// schedule a heartbeat to occur every `us` microseconds
int hb_repeat(uint64_t us, hb_callback_t callback);

// cancel the scheduled heartbeat, returning zero (false) if
// there was not one scheduled.
int hb_cancel(void);
