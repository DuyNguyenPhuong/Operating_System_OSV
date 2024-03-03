#!/bin/bash

# Compile C program
gcc -o cache cache.c helperMeasurement.c

# declare -a sizes=(1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864 134217728 268435456)
declare -a sizes=(268435456)

# Print CSV header
echo "Array Size (KB),Average Access Time (ns)" > newRes.csv

# Loop through the sizes array
for size in "${sizes[@]}"; do
    # Run C program with current size and capture output
    output=$(./cache $size)
    
    # Print output in CSV format. Assuming the output is already in the desired format.
    echo "$size,$output" >> newRes.csv
done

echo "Results stored in newRes.csv"
