#!/bin/bash

times=()

for i in {1..1000}; do
    t=$(./wasmtime-my_test | awk '{print $3}')
    times+=($t)
done

sorted=($(printf '%s\n' "${times[@]}" | sort -n))
len=${#sorted[@]}
if (( len % 2 == 1 )); then
    median=${sorted[$((len/2))]}
else
    mid=$((len/2))
    median=$(( (sorted[mid-1] + sorted[mid]) / 2 ))
fi

echo "Median execution time (ns): $median"
