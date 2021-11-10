# Linux Kernel Heartbeat Scheduler



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
