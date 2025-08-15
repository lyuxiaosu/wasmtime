#!/bin/bash
set -e

times=()

for i in {1..1000}; do
    # 取第三列数字（ns），再转成 us
    t=$(./wasmtime-my_test | awk '{print $3}')
    t_us=$((t / 1000))
    times+=($t_us)
done

# 排序
sorted=($(printf '%s\n' "${times[@]}" | sort -n))
len=${#sorted[@]}

# 中位数
if (( len % 2 == 1 )); then
    median=${sorted[$((len/2))]}
else
    mid=$((len/2))
    median=$(( (sorted[mid-1] + sorted[mid]) / 2 ))
fi

# 最小值 & 最大值
min=${sorted[0]}
max=${sorted[-1]}

echo "Min execution time (us): $min"
echo "Max execution time (us): $max"
echo "Median execution time (us): $median"
