#ifndef ACCESSARRAY_H
#define ACCESSARRAY_H

#define NUM_INT_IN_CACHE_LINE 16

void accessArray(int *array, int numElements, int random_increment);
void set_cpu_affinity();
// void start_timer(struct timespec *start);
// void stop_timer(struct timespec *start, struct timespec *end);
// double measure_average_access_time(int *array, int numElements, int randomNumber2);
// int initialize_array(int **array, long long size);

#endif /* ACCESSARRAY_H */
