#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include "accessArray.h"
// #include "lib/test.h"

#define NUM_TRIALS 100
#define NUM_INT_IN_CACHE_LINE 16
#define ONE_BILLION 1000000000LL

// Main function to compare access times
int main()
{
    struct timespec start, end;
    long long elapsedTimeSmall, elapsedTimeLarge;
    int dummy = 0;

    // Size definitions for comparison
    int smallSize = 1024;             // 1KB
    int largeSize = 1024 * 1024 * 64; // 64MB

    // Prepare for CPU affinity
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
    {
        perror("sched_setaffinity failed");
        exit(EXIT_FAILURE);
    }

    // Allocate and initialize arrays
    int *smallArray = (int *)malloc(smallSize);
    int *largeArray = (int *)malloc(largeSize);
    if (!smallArray || !largeArray)
    {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize arrays with some values
    for (int i = 0; i < smallSize / sizeof(int); i++)
    {
        smallArray[i] = i;
    }
    for (int i = 0; i < largeSize / sizeof(int); i++)
    {
        largeArray[i] = i;
    }

    // Access small array
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int trial = 0; trial < NUM_TRIALS; trial++)
    {
        accessArray(smallArray, smallSize / sizeof(int), 1);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsedTimeSmall = (end.tv_sec - start.tv_sec) * ONE_BILLION + (end.tv_nsec - start.tv_nsec);

    // Access large array
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int trial = 0; trial < NUM_TRIALS; trial++)
    {
        accessArray(largeArray, largeSize / sizeof(int), 1);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsedTimeLarge = (end.tv_sec - start.tv_sec) * ONE_BILLION + (end.tv_nsec - start.tv_nsec);

    if (elapsedTimeSmall > elapsedTimeLarge)
    {
        printf("Error: Access Time Small is less than Access Time Large\n");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;

    // Clean up
    free(smallArray);
    free(largeArray);

    return 0;
}
