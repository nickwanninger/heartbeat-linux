#!/usr/bin/env bash

set -e


mkdir -p build
# compile the 
clang -O3 -g0 -I../include -S sum_array.c -o build/sum_array.s

python3 transform.py build/sum_array.s build/sum_array.rf.s
clang build/sum_array.rf.s ../src/entry.S -o build/sum_array
