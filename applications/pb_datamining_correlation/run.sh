#!/bin/bash
set -e

times=()

for i in {1..10000}; do
    t=$(./pb_datamining_correlation.native | awk '{print $3}')
    t_us=$((t / 1000))
    times+=($t_us)
done

sorted=($(printf '%s\n' "${times[@]}" | sort -n))
len=${#sorted[@]}

if (( len % 2 == 1 )); then
    median=${sorted[$((len/2))]}
else
    mid=$((len/2))
    median=$(( (sorted[mid-1] + sorted[mid]) / 2 ))
fi

min=${sorted[0]}
max=${sorted[-1]}

echo "Min execution time (us): $min"
echo "Max execution time (us): $max"
echo "Median execution time (us): $median"
