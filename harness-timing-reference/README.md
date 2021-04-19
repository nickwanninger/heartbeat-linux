# Producer-Consumer Lab
Copyright (c) 2019 Peter Dinda

The purpose of this lab is to introduce the student to the challenges of
designing and implementing a high performance concurrent data structure. Both
thread and interrupt concurrency are included.

The specific data structure is a producer-consumer queue implemented as a
classic ring-buffer. The driver includes control over the number of producer
and consumer threads, as well as whether interrupt handlers can be producers or
consumers. Rings are created with possible limits on the number of producers
and consumers, which makes it possible to focus on SPSC, SPMC, MPPC, and MPMC
models.

Test your implementation by running `./harness`. Run `harness -h` to see the
available options.

## Files

 * `atomics.[ch]` - wrappers for hardware atomic operations
 * `ring.h`       - required ring interface for producer-consumer problem
 * `ring.c`       - implementation of ring *without* concurrency control
 * `harness.c`    - test harness 
 * `config.h`     - configuration, debug macros
 * `Makefile`     - build rules

Be aware that we will overwrite `harness.c`, `config.h`, and `Makefile` with
their default versions before running your code. We will also disable debug
prints in the `config.h` file (`#define DEBUG_OUTPUT 0`). Make sure that all of
your modifications are in `atomics.[ch]` or `ring.[ch]` and that the code runs
with the original `harness.c` implementation.

