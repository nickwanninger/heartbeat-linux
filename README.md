# Linux Kernel Heartbeat Scheduler

Delivering timer "interrupts" to userspace in the least hygienic way.

## Building
This project is comprised of two parts: a kernel module to allow low latency heartbeats
to be delivered to the userspace, and a userspace library designed to interface with the
kernel module. There are also a set of example programs that use the library.


To compile the library:
```
$ make
```

To compile the kernel module:
```
$ make kmod
```

You can also insert the kernel module using `make ins` and `make rm` to remove it.



## Usage

Once you have it built, the entire API is located in `include/heartbeat.h`. On each thread that
wants to receive heartbeat interrupts, you must call `hb_init(core)`, which locks the thread
to a certain core. We lock the thread to a core to avoid the overhead of an IPI or signalling.

Once initialized, a heartbeat can be scheduled using `hb_oneshot(us, cb)`, where after `us` 
microseconds (±~1us), the function `cb` will be called. You must schedule another heartbeat
afterwards when using this interface.

`hb_repeat(us, cb)` works in a similar way, but you do not need to call `hb_oneshot` after
`cb` is called, resulting in almost no overhead. Note, however, than extremely low intervals
using `repeat` results in missed interrupts, as a heartbeat may occur during your callback,
and reentrancy is not supported.

Before exiting the thread using heartbeats, be sure to call `hb_exit()`