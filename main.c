#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include "test_cpu.h"
// #include "cpu.h"
// #include "memory.h"


// #define CYCLES_IN_ONE_STEP CPU_FREQ * 10
// #define STEP_DURATION_IN_MICROS 100 * 1000

// long diff(struct timespec start_time, struct timespec end_time) {
//     long diff_nanos = (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
//     return diff_nanos / 1000; // Convert to microseconds
// }

// void run_test(char *program_path) {
//     memory_read_file(program_path, 0x100);
//     cpu_init();
//     cpu_set_PC_reg(0x100);
//     memory_store(0x0005, 0xC9); // Insert return operation at address on which CP/M's print subroutine should start
//     bool should_run = true;
//     struct timespec start_time, end_time;
//     long time_delta;
//     int cycles_elapsed = 0;
//     while (should_run) {
//         clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
//         while (cycles_elapsed < CYCLES_IN_ONE_STEP) {
//             cycles_elapsed += cpu_step();
//             if (cpu_get_PC_reg() == 0x0005) {
//                 bdos_io();
//             } else if (cpu_get_PC_reg() == 0x0000) { // CP/M resets on this address so for now we can exit
//                 should_run = false;
//             }
//         }
//         cycles_elapsed = 0;
//         clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
//         time_delta = diff(start_time, end_time);
//         if (time_delta < STEP_DURATION_IN_MICROS) {
//             usleep(STEP_DURATION_IN_MICROS - time_delta);
//         }
//     }
// }

int main() {
    run_all_tests();
    return 0;
}
