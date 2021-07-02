#ifndef __CPU_H__
#define __CPU_H__

#include <stdint.h>

typedef union REG_PAIR {
    uint16_t single;
    struct REG_PAIR_INNER {
        uint8_t lower;
        uint8_t upper;
    } pair;
} reg_pair_t;

typedef union STATUS_REG {
    uint8_t single;
    struct STATUS_REG_INNER {
        int C: 1; // carry
        int _padding1: 1;
        int P: 1; // parity
        int _padding2: 1;
        int AC: 1; // auxiliary carry
        int _padding3: 1;
        int S: 1; // sign
        int Z: 1; // zero
    } flags;
} status_reg_t;

void test();

#endif // __CPU_H__