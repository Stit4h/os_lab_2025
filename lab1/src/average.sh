#!/bin/bash

count=$#
sum=0

for num in "$@"; do
    sum=$((sum + num))
done

echo "Количество: $count"
echo "Среднее: $((sum / count))"