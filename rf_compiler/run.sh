#!/usr/bin/env bash

set -e


mkdir -p build
# compile the 
clang -O3 -g0 -S main.c -o build/main.s

python3 transform.py build/main.s build/main.rf.s
clang build/main.rf.s -o build/main
