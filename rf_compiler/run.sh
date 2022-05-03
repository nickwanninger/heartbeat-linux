#!/usr/bin/env bash

set -e


mkdir -p build
# compile the source code to asm
clang -O3 -g0 -I../include -S sum_array.c -o build/sum_array.s
# transform it
python3 transform.py build/sum_array.s build/sum_array.rf.s
# make a binary
clang build/sum_array.rf.s ../src/entry.S -o build/sum_array
