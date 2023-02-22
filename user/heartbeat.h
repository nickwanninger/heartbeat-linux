#pragma once

// This header defines the interface by which a userspace process can
// request a heartbeat interval

#include <stdint.h>
#include "./heartbeat_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void hb_set_rollforwards(struct hb_rollforward *rfs, unsigned count);
int hb_init(void);  // open the hb driver and initialize it for this process
void hb_exit(void); // close the hb driver (freeing any state we allocated)
int hb_oneshot(uint64_t us); // Setup a oneshot timer
int hb_repeat(uint64_t us);  // Setup a repeat timer
int hb_cancel(void);         // Cancel any timer

#ifdef __cplusplus
}
#endif
