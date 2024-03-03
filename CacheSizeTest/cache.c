#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include "accessArray.h"

#define NUM_TRIALS 10000         // Set Number Trial
#define NUM_INT_IN_CACHE_LINE 16 // Cache size is 64B so there is 16 int in a cache line
#define ONE_BILLION 1000000000LL // Set one Billion

// This definition is to comparing L1 and L2 Cache
#define MIN_SIZE 1024      // Start with 1KB
#define MAX_SIZE 1024 * 64 // End up to 64KB
#define STEP_SIZE 1024     // Step size if we want to list all size

// This definition is to comparing L2 and L3 Cache
// #define MIN_SIZE 1024 * 1024      // Start with 1 MB
// #define MAX_SIZE 1024 * 1024 * 50 // End up to 50 MB
// #define STEP_SIZE 1024 * 1024 // Step size if we want to list all size

int main()
{
    set_cpu_affinity();

    // Time variable to keep track of time
    struct timespec start, end;

    // Dummy integer to print at the end to prevent complier optimization
    int dummy = 0;

    // Go through each size of array from min to max size
    for (long long size = MIN_SIZE; size <= MAX_SIZE; size += STEP_SIZE)
    {
        // Set time variable to 0
        start.tv_nsec = 0;
        end.tv_nsec = 0;
        start.tv_sec = 0;
        end.tv_sec = 0;

        // Set number of element in a array
        long long numElements = size / sizeof(int);

        // Malloc an array
        int *array = (int *)malloc(size);
        if (!array)
        {
            perror("Memory allocation failed");
            return 1;
        }

        // Initialize array to ensure it'll present in physical memory
        // I tried to *(random%15) + 1 to make sure the the complier actually initilize the array
        srand(time(NULL));
        int randomNumber1 = rand();
        int randomNumber2 = rand();
        for (int i = 0; i < numElements; i++)
        {
            array[i] = i * (randomNumber1 % 15) + 1;
        }

        // Start the clock
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int trial = 0; trial < NUM_TRIALS; trial++)
        {
            accessArray(array, numElements, randomNumber2);
        }

        // End the clock
        clock_gettime(CLOCK_MONOTONIC, &end);

        // Calculate the elapse time by nanosecond
        long long elapsedTime = (end.tv_sec - start.tv_sec) * ONE_BILLION + (end.tv_nsec - start.tv_nsec);

        // Calculate the total access time
        long long totalAccessTime = NUM_TRIALS * numElements / NUM_INT_IN_CACHE_LINE;

        // printf("Elapsed Time is %lld \n", elapsedTime);

        // printf("Total size is %lld \n", size);

        // printf("Total element is %lld \n", numElements);

        // printf("Total loop is %lld \n", NUM_TRIALS * numElements / 16);

        double averageAccessTime = (double)elapsedTime / totalAccessTime;

        dummy += array[0] + array[randomNumber1 % (numElements)];

        printf("%.2f,%.2f\n", (double)size / 1024, averageAccessTime);

        free(array);
    }

    printf("Dummy output to prevent complier optimization: %d\n", dummy);

    return 0;
}