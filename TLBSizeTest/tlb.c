#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PAGESIZE sysconf(_SC_PAGESIZE)

int main(int argc, char *argv[])
{
    // Check the input
    if (argc != 3)
    {
        printf("Wrong input. It should be: %s <numPages> <numTrials>\n", argv[0]);
        return 1;
    }

    printf("Page Size is: %ld\n", PAGESIZE); // 4096 // 4kB

    long long numPages = atoi(argv[1]);
    long long numTrials = atoi(argv[2]);
    long long jump = PAGESIZE / sizeof(int);

    jump = 16;

    printf("Jump Size is: %d\n", jump);

    printf("Array Size is: %ld\n", numPages * PAGESIZE);
    struct timespec start, end;
    long long totalTime = 0;

    int *array = (int *)malloc(numPages * PAGESIZE);
    if (!array)
    {
        perror("Memory allocation failed");
        return 1;
    }

    long long sizeInByte = numPages * PAGESIZE;
    long long numElements = sizeInByte / 4;
    // int size = numPages * jump;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Touch each page in the array
    for (int trial = 0; trial < numTrials; ++trial)
    {
        // clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < numElements; i += jump)
        {
            array[i] += 1;
        }
        // clock_gettime(CLOCK_MONOTONIC, &end);

        // totalTime += (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    totalTime = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);

    printf("Tong total Time is %lld \n", totalTime);

    // Use a[0] to make sure the compiler doesn't optimize away the loop
    int dummny = array[0] + 1;
    // double averageAccessTime = (double)totalTime / (numTrials * numPages);

    double averageAccessTime = (double)totalTime / (numTrials * numElements / jump);

    printf("Total Access Time %lld \n", numTrials * numElements / jump);
    printf("%lld,%lld,%.2f\n", numPages, numTrials, averageAccessTime);

    free(array);
    return 0;
}
