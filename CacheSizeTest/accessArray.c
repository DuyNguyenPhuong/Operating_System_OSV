#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>

#define NUM_INT_IN_CACHE_LINE 16

// Access step to with a step of cache line size
// Because we don't want to access the element in the same cache line
// Then we add a random number to each element
void accessArray(int *array, int numElements, int random_increment)
{
    for (int i = 0; i < numElements; i += NUM_INT_IN_CACHE_LINE)
    {
        array[i] += random_increment;
    }
}

// This code is to set CPU Affinity
// This code is reference by https://stackoverflow.com/questions/280909/how-to-set-cpu-affinity-for-a-process-from-c-or-c-in-linux
void set_cpu_affinity()
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
}
