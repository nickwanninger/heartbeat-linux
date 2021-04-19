/*
 *
 *  Producer-Consumer Lab
 *
 *  Copyright (c) 2019 Peter A. Dinda
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <signal.h>

#include "config.h"
#include "atomics.h"

//
// The interrupt_disable_save and interrupt_enable_restore functions
// emulate the interrupt control we have within an actual kernel
// using the analogous mechanisms in userspace, namely, signal
// masking
//

// disables interrupts, saving previous interrupt state
void interrupt_disable_save(int* oldstate) {
  sigset_t cur;
  sigemptyset(&cur);

  sigprocmask(SIG_BLOCK, 0, &cur); // get current mask

  *oldstate = sigismember(&cur, INTERRUPT_SIGNAL); // stash "interrupt" state

  sigaddset(&cur, INTERRUPT_SIGNAL); // mask "interrupt"

  sigprocmask(SIG_BLOCK, &cur, 0); // set new mask
}


// restores state of interrupts from argument
void interrupt_enable_restore(int oldstate) {
  if (oldstate == 0) {
    // need to unblock
    sigset_t cur;
    sigemptyset(&cur);

    sigprocmask(SIG_BLOCK, 0, &cur); // get current mask

    sigdelset(&cur, INTERRUPT_SIGNAL); // unmask "interrupt"

    sigprocmask(SIG_BLOCK, &cur, 0); // set new mask
  }

  // otherwise there is nothing to do - it should stay blocked
}

