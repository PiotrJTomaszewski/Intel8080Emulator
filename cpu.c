#include <stdio.h>
#include <string.h>
#include "cpu.h"

static uint8_t regA;
static reg_pair_t regBC, regDE, regHL;
static uint16_t regSP, regPC;
static status_reg_t status_reg;

void test() {
    reg_pair_t regBC;
    regBC.single = 0x1234;
    printf("%x\n", regBC.pair.upper);
    printf("%x\n", regBC.pair.lower);

    status_reg_t status_reg;
    status_reg.single = 0;
    status_reg.flags.S = 1;
    status_reg.flags.Z = 1;
    status_reg.flags.AC = 1;
    status_reg.flags.P = 1;
    status_reg.flags.C = 1;
    printf("%x\n", status_reg.single);

}