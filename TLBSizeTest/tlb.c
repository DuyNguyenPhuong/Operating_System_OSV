#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
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

    printf("Page size is: %ld \n", PAGESIZE);

    int numPages = atoi(argv[1]);

    printf("Num page is: %d \n", numPages);
    int numTrials = atoi(argv[2]);
    int jump = PAGESIZE / sizeof(int);
    struct timeval start, end;
    long totalTime = 0;

    int *a = (int *)malloc(numPages * PAGESIZE);
    if (!a)
    {
        free(a);
        perror("Memory allocation failed");
        return 1;
    }

    // Touch each page in the array
    for (int trial = 0; trial < numTrials; ++trial)
    {
        gettimeofday(&start, NULL);
        for (int i = 0; i < numPages * jump; i += jump)
        {
            a[i] += 1;
        }
        gettimeofday(&end, NULL);

        totalTime += (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    }
    printf("Print a[0]: %d \n", a[0]);
    printf("Average access time: %f microseconds\n", (double)totalTime / (numTrials * numPages));

    free(a);
    return 0;
}

// Note:
// Use: clock get time
// Use script to run the test: echo >> hello.txt
