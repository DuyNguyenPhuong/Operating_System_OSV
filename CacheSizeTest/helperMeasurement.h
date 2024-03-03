#ifndef HELPERMEASUREMENT_H
#define HELPERMEASUREMENT_H

#define NUM_TRIALS 100           // Set Number Trial
#define NUM_INT_IN_CACHE_LINE 16 // Cache size is 64B so there is 16 int in a cache line
#define ONE_BILLION 1000000000LL // Set one Billion

void accessArray(int *array, int numElements, int random_increment);
void set_cpu_affinity();
double measure_average_access_time(int *array, long long numElements, int randomIncrementNumber);
long long calculate_num_elements(long long size);
int malloc_and_randomly_initialize_array(int **array, long long size, long long numElements);

#endif /* HELPERMEASUREMENT_H */