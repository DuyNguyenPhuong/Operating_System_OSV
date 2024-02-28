#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>

#define NUM_TRIALS 10
#define MIN_SIZE 512               // Start with 1KB
#define MAX_SIZE 1024 * 1024 * 512 // End up to 512MB
#define CACHE_SIZE 64              // Cache size is 64B
#define STEP_SIZE 1024 * 1024      // Step size if we want to list all size

void accessArray(int *array, int size, int incre)
{
    // Access step to with a step of cache line size
    // Because we don't want to access the element in the same cache line
    for (int i = 0; i < size; i += CACHE_SIZE)
    {
        array[i] += incre;
        // array[i] = array[array[i] % size];
    }
}

int main()
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    // Bind process to CPU 0
    CPU_SET(0, &mask);

    // Set CPU affinity
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
    {
        perror("sched_setaffinity failed");
        exit(EXIT_FAILURE);
    }
    struct timespec start, end;
    int dummy = 0;
    // for (int size = START_SIZE; size <= MAX_SIZE; size += STEP_SIZE)
    for (long long size = MIN_SIZE; size <= MAX_SIZE; size *= 2)
    {
        start.tv_nsec = 0;
        end.tv_nsec = 0;
        start.tv_sec = 0;
        end.tv_sec = 0;

        int numElements = size / sizeof(int);
        int *array = (int *)malloc(size);

        if (!array)
        {
            perror("Memory allocation failed");
            return 1;
        }

        // Initialize array --> ensure it'll present in physical memory
        // I tried to *3 + 1 to make sure that the it actually present in physical memory
        // Because I afraid if we do arr[i] = i the complier will do some fancy stuff

        srand(time(NULL)); // Initialization, should only be called once.
        int r = rand();    // Returns a pseudo-random integer between 0 and RAND_MA
        int r1 = rand();
        for (int i = 0; i < numElements; i++)
        {
            array[i] = i * (r % 10) + 1;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int trial = 0; trial < NUM_TRIALS; trial++)
        {
            accessArray(array, numElements, r1);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        int numAccessedElement = (int)(size + 63) / CACHE_SIZE;
        long long elapsedTime = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
        // printf("Elapsed Time is %lld \n", elapsedTime);

        // printf("Total loop is %lld \n", NUM_TRIALS * size / 64);

        double averageAccessTime = (double)elapsedTime / (long long)(NUM_TRIALS * numAccessedElement);

        dummy += array[0];
        // printf("######################This is dummy: %d \n", dummy);
        printf("%lld,%.2f\n", size / 1024, averageAccessTime);

        free(array);
    }

    return 0;
}