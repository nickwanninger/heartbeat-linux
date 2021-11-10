#pragma once

// This header defines the interface by which a userspace process can
// request a heartbeat interval


#include "heartbeat_kmod.h"
#include <stdint.h>


typedef void (*hb_callback_t)(void);

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
