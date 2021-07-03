#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "cpu.h"
#include "memory.h"

#define CYCLES_IN_ONE_STEP CPU_FREQ * 10
#define STEP_DURATION_IN_MICROS 100 * 1000

long diff(struct timespec start_time, struct timespec end_time) {
    long diff_nanos = (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
    return diff_nanos / 1000; // Convert to microseconds
}

int main(int argc, char **argv) {
    // memory_read_file("../8kBas_e0.bin");
    memory_read_file("../TEST.COM");
    cpu_init();
    struct timespec start_time, end_time;
    long time_delta;
    int cycles_elapsed = 0;
    while (1) {
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
        while (cycles_elapsed < CYCLES_IN_ONE_STEP) {
            cycles_elapsed += cpu_step();
        }
        cycles_elapsed = 0;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
        time_delta = diff(start_time, end_time);
        if (time_delta < STEP_DURATION_IN_MICROS) {
            usleep(STEP_DURATION_IN_MICROS - time_delta);
        }
    }
    return 0;
}
