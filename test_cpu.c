#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include "cpu.h"
#include "memory.h"


/**
 * Implement some IO functions of BDOS from CP/M.
 * Tests were designed to be run inside CP/M
 * but they're using it only for printing
 * so we can emulate that function and forget about CP/M :)
 */
static void bdos_io() {
    uint8_t c_reg = cpu_get_C_reg();
    if (c_reg == 2) {
        printf("%c", cpu_get_E_reg());
    } else if (c_reg == 9) {
        uint16_t addr = cpu_get_DE_reg();
        char chr = 0;
        while (1) {
            chr = memory_get(addr++);
            if (chr == '$')
                break;
            printf("%c", chr);
        }
    }
}

void run_test(char *program_path) {
    printf("====== Running test %s ======\n", program_path);
    memory_read_file(program_path, 0x100);
    cpu_init();
    cpu_set_PC_reg(0x100);
    memory_store(0x0005, 0xC9); // Insert return operation at address on which CP/M's print subroutine should start
    bool should_run = true;
    long long total_cycles_elapsed = 0;
    while (should_run) {
        total_cycles_elapsed += cpu_step();
        if (cpu_get_PC_reg() == 0x0005) {
            bdos_io();
        } else if (cpu_get_PC_reg() == 0x0000) { // CP/M resets on this address so for now we can exit
            should_run = false;
        }
    }
    printf("\n====== Elapsed CPU cycles: %lld ======\n\n\n", total_cycles_elapsed);
}

void run_all_tests() {
    char *Test_Programs[] = {
        "../programs/8080PRE.COM",
        "../programs/CPUTEST.COM",
        "../programs/TST8080.COM",
        "../programs/8080EXER.COM",
        "../programs/8080EXM.COM"
    };
    for (unsigned i = 0; i < (sizeof(Test_Programs) / sizeof(Test_Programs[0])); i++) {
        run_test(Test_Programs[i]);
    }
}
