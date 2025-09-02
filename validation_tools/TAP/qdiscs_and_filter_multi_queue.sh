#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <number_of_filters>"
    exit 1
fi

sudo tc qdisc replace dev tap0 root handle 1: multiq

for i in $(seq 1 $1); do
    queue_mapping=$((i - 1))
    sudo tc filter add dev tap0 parent 1: protocol ip prio 1 u32 match ip dport $i 0xffff action skbedit queue_mapping $queue_mapping
done
