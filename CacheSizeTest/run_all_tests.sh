#!/bin/bash

# Function to compile C programs
compile() {
    echo "Compiling $1..."
    if ! gcc -o "$1" "$1.c" accessArray.c; then
        echo "Compilation failed for $1"
        exit 1
    fi
}

# Function to run test cases
run_test() {
    test_case="$1"
    printf "Running test: $test_case...\n"
    if ./$test_case; then
        echo -e "\e[32mPassed\e[0m $test_case"
        ((passed_tests++))  # Increment passed tests count
    else
        echo -e "\e[31mFailed\e[0m $test_case"
    fi
}

# Trap to ensure cleanup
cleanup() {
    rm -f small-large-array
}
trap cleanup EXIT

# Compile small-large-array.c
compile small-large-array

# Initialize count of passed tests
passed_tests=0
total_tests=4

# Run test cases
run_test small-large-array
# run_test test_case_1
# run_test test_case_2
# run_test test_case_3


echo "Number of passed tests: $passed_tests / $total_tests"


# chmod +x run_all_tests.sh
# ./run_all_tests.sh
