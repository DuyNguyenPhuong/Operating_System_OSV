#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
// #include <bits/time.h>
// #include <linux/time.h>

#define PAGESIZE sysconf(_SC_PAGESIZE)

int main(int argc, char *argv[])
{
    // Check the input
    if (argc != 3)
    {
        printf("Wrong input. It should be: %s <numPages> <numTrials>\n", argv[0]);
        return 1;
    }

    int numPages = atoi(argv[1]);
    int numTrials = atoi(argv[2]);
    int jump = PAGESIZE / sizeof(int);
    struct timespec start, end;
    long long totalTime = 0;

    int *a = (int *)malloc(numPages * PAGESIZE);
    if (!a)
    {
        perror("Memory allocation failed");
        return 1;
    }

    // Touch each page in the array
    for (int trial = 0; trial < numTrials; ++trial)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < numPages * jump; i += jump)
        {
            a[i] += 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);

        totalTime += (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
    }

    // Use a[0] to make sure the compiler doesn't optimize away the loop
    int dummny = a[0] + 1;
    double averageAccessTime = (double)totalTime / (numTrials * numPages);
    printf("%d,%d,%.2f\n", numPages, numTrials, averageAccessTime);

    free(a);
    return 0;
}
