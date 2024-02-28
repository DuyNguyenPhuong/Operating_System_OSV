#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_TRIALS 10
#define MIN_SIZE 1024              // Start with 1KB
#define MAX_SIZE 1024 * 1024 * 128 // End up to 64MB
#define CACHE_SIZE 64              // Cache size is 64B
#define STEP_SIZE 1024 * 1024      // Step size if we want to list all size

void accessArray(int *array, int size)
{
    // Access step to with a step of cache line size
    // Because we don't want to access the element in the same cache line
    for (int i = 0; i < size; i += CACHE_SIZE)
    {
        array[i] += 1;
    }
}

int main()
{
    struct timespec start, end;
    int dummy = 0;
    // for (int size = START_SIZE; size <= MAX_SIZE; size += STEP_SIZE)
    for (int size = MIN_SIZE; size <= MAX_SIZE; size *= 2)
    {
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
        for (int i = 0; i < numElements; i++)
        {
            array[i] = i * 3 + 1;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int trial = 0; trial < NUM_TRIALS; trial++)
        {
            accessArray(array, numElements);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        long long elapsedTime = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
        double averageAccessTime = elapsedTime / (double)(NUM_TRIALS * numElements);
        dummy += array[0];
        printf("%d,%.2f\n", size / 1024, averageAccessTime);

        free(array);
    }

    return 0;
}
