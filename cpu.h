#ifndef __CPU_H__
#define __CPU_H__

#include <stdint.h>
#include <stdbool.h>

typedef union REG16BIT {
    uint16_t single;
    struct REG16BIT_BYTE_PAIR {
        uint8_t lower;
        uint8_t higher;
    } pair;
} reg_16bit_t;

typedef union STATUS_REG {
    uint8_t single;
    struct STATUS_REG_INNER {
        unsigned C: 1; // carry
        unsigned _unused1: 1;
        unsigned P: 1; // parity
        unsigned _unused2: 1;
        unsigned AC: 1; // auxiliary carry
        unsigned _unused3: 1;
        unsigned S: 1; // sign
        unsigned Z: 1; // zero
    } flags;
} status_reg_t;

typedef struct CPU_STATE {
    bool interrupts_enabled;
    bool halted;
} cpu_state_t;

void cpu_exec_op(uint8_t opcode);

void test();

#endif // __CPU_H__