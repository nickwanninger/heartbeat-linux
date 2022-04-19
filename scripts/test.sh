#!/bin/bash

set -e

echo "# method, interval, iters_per_proc, total_iters"

INTERVAL=50

for i in $(seq 1 10)
do
	sudo build/ex h $INTERVAL
done

for i in $(seq 1 10)
do
	sudo build/ex p $INTERVAL
done
