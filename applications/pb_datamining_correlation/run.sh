#!/bin/bash

# 存储所有执行时间
times=()

# 执行 1000 次
for i in {1..1000}; do
    # 捕获输出，提取时间（假设输出格式是 "Execution time: <num> ns"）
    t=$(./pb_datamining_correlation.native | awk '{print $3}')
    times+=($t)
done

# 排序并取中位数
sorted=($(printf '%s\n' "${times[@]}" | sort -n))
len=${#sorted[@]}
if (( len % 2 == 1 )); then
    # 奇数个元素，中位数是中间值
    median=${sorted[$((len/2))]}
else
    # 偶数个元素，中位数是中间两个的平均
    mid=$((len/2))
    median=$(( (sorted[mid-1] + sorted[mid]) / 2 ))
fi

echo "Median execution time (ns): $median"
