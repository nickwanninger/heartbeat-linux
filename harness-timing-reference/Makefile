#
#  Producer-Consumer Lab
#
#  Copyright (c) 2019 Peter A. Dinda
#
#  Warning: This file will be overwritten before grading.
#

## Command to compile code. Change to `-Og` version for debugging with GDB
#CC = gcc -Wall -Og -g -pthread
CC = gcc -Wall -v -O3 -pthread

all: harness

harness: harness.o ring.o atomics.o
	$(CC) harness.o ring.o atomics.o -lm -lrt -o harness

harness.o: harness.c ring.h atomics.h config.h
	$(CC) -c harness.c

ring.o : ring.c ring.h atomics.h config.h
	$(CC) -c ring.c

atomics.o : atomics.c atomics.h config.h

clean:
	rm -f *.o harness
