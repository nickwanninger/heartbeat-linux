#!/usr/bin/env bash
# ./bench.sh <ncores> | tee results.csv

set -e

./run.sh

NCORES=$1
CORES=( $(seq 1 2 $NCORES ) )
INPUTSZS=(4096 4194304)
INTERVALS=(100 400)
ph=( "p" "h" )

echo "interrupt, nthreads, sum, handler_calls, block_sz, array_sz, interval_us"
for cfg in "${ph[@]}"; do
    for P in ${CORES[@]}; do
	for isz in "${INPUTSZS[@]}"; do
	    for interval in ${INTERVALS[@]}; do
		./build/sum_array $isz $cfg $P $interval
	    done
	done
    done
done
