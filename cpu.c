#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "memory.h"
#include "io.h"
#include "debug.h"

// TODO: Implement CPU "pins", like processor state

#define regBC _regBC.single
#define regB  _regBC.pair.higher
#define regC  _regBC.pair.lower
#define regDE _regDE.single
#define regD  _regDE.pair.higher
#define regE  _regDE.pair.lower
#define regHL _regHL.single
#define regH  _regHL.pair.higher
#define regL  _regHL.pair.lower
#define regPC _regPC.single
#define regPC_higher _regPC.pair.higher
#define regPC_lower _regPC.pair.lower
#define regSP _regSP.single
#define regSP_higher _regSP.pair.higher
#define regSP_lower _regSP.pair.lower

uint8_t regA;
reg_16bit_t _regBC, _regDE, _regHL, _regPC, _regSP; // Don't use directly, use defines instead
status_reg_t status_reg;
cpu_state_t cpu_state;

inline static uint16_t join_bytes(uint8_t higher, uint8_t lower) {
    return (higher << 8) | lower;
}

inline static void calc_set_Z_flag(uint8_t val) {
    status_reg.flags.Z = (val == 0);
}

inline static void calc_set_S_flag(uint8_t val) {
    status_reg.flags.S = ((val & (1 << 7)) == 0x80);
}

inline static void calc_set_P_flag(uint8_t val) {
    status_reg.flags.P = (__builtin_popcount(val) % 2 == 0);
}

static uint8_t add8bit_with_flags(uint8_t val1, uint8_t val2, uint8_t carry) {
    int result = val1 + val2 + carry;
    uint8_t result8bit = result & 0xFF;
    status_reg.flags.C = (result >= 0x100);
    status_reg.flags.AC = (((val1 & 0x0F) + (val2 & 0x0F) + carry) > 0x0F);
    calc_set_Z_flag(result8bit);
    calc_set_S_flag(result8bit);
    calc_set_P_flag(result8bit);
    return result8bit;
}

/**
 * Affected flags: C
 */
static uint16_t add16bit_with_flag(uint16_t val1, uint16_t val2) {
    int result = val1 + val2;
    status_reg.flags.C = (result >= 0x10000);
    return result & 0xFFFF;
}

static uint8_t sub8bit_with_flags(uint8_t val1, uint8_t val2, uint8_t borrow) {
    int result = val1 - val2 - borrow;
    uint8_t result8bit = result & 0xFF;
    status_reg.flags.C = ((val2+borrow) > val1);
    status_reg.flags.AC = (((val1 & 0x0F) + (~val2 & 0x0F) + (borrow^1)) > 0x0F);
    calc_set_Z_flag(result8bit);
    calc_set_S_flag(result8bit);
    calc_set_P_flag(result8bit);
    return result8bit;
}

/**
 * Affected flags: Z, S, P, AC
 */
static uint8_t inc8bit_with_flags(uint8_t val) {
    uint8_t result = (val + 1) & 0xFF;
    status_reg.flags.AC = (((val & 0x0F) + 1) > 0x0F);
    calc_set_Z_flag(result);
    calc_set_S_flag(result);
    calc_set_P_flag(result);
    return result;
}

/**
 * Affected flags: Z, S, P, AC
 */
static uint8_t dec8bit_with_flags(uint8_t val) {
    uint8_t result = (val - 1) & 0xFF;
    /* The whole calculation here should look like the one in 'sub8bit_with_flags'
     * but because 'val2' is always equal 1 and 'borrow' is always equal 0
     * we can write ((val1 & 0x0F) + (~1 & 0x0F) + (0^1)) > 0x0F
     * and the two latter values are equal: 0x0E + 1 = 0x0F.
     * So in the end it can be simplified to (val & 0x0F) + 0x0F > 0x0F
    */ 
    status_reg.flags.AC = ((val & 0x0F) > 0);
    calc_set_Z_flag(result);
    calc_set_S_flag(result);
    calc_set_P_flag(result);
    return result;
}

static uint8_t and8bit_with_flags(uint8_t val1, uint8_t val2) {
    uint8_t result = val1 & val2;
    calc_set_Z_flag(result);
    calc_set_S_flag(result);
    calc_set_P_flag(result);
    status_reg.flags.C = 0;
    status_reg.flags.AC = (((val1 | val2) & 0x08) != 0);
    return result;
}

static uint8_t or8bit_with_flags(uint8_t val1, uint8_t val2) {
    uint8_t result = val1 | val2;
    calc_set_Z_flag(result);
    calc_set_S_flag(result);
    calc_set_P_flag(result);
    status_reg.flags.C = 0;
    status_reg.flags.AC = 0;
    return result;
}

static uint8_t xor8bit_with_flags(uint8_t val1, uint8_t val2) {
    uint8_t result = val1 ^ val2;
    calc_set_Z_flag(result);
    calc_set_S_flag(result);
    calc_set_P_flag(result);
    status_reg.flags.C = 0;
    status_reg.flags.AC = 0;
    return result;
}

inline static void stack_push(uint8_t value) {
    memory_store(--regSP, value);
}

inline static uint8_t stack_pop() {
    return memory_get(regSP++);
}

inline static uint8_t get_next_prog_byte() {
    return memory_get(regPC++);
}

static uint16_t get_next_2_prog_bytes() {
    uint8_t lower = memory_get(regPC++);
    uint8_t higher = memory_get(regPC++);
    return join_bytes(higher, lower);
}

/**
 * returns number of cycles this operation takes
 */
inline static int cond_return(bool condition) {
    if (condition) {
        regPC_lower = stack_pop();
        regPC_higher = stack_pop();
        return 11;
    } else {
        return 5;
    }
}

/**
 * Jump address is read from program memory
 */
inline static void cond_jump(bool condition) {
    if (condition) {
        regPC = get_next_2_prog_bytes();
    } else {
        regPC += 2;
    }
}

/**
 * Called address is read from program memory
 * returns number of cycles this operation takes 
 */
static int cond_call(bool condition) {
    if (condition) {
        uint16_t newPC = get_next_2_prog_bytes();
        stack_push(regPC_higher);
        stack_push(regPC_lower);
        regPC = newPC;
        return 17;
    } else {
        regPC += 2;
        return 11;
    }
}

inline static void call_addr(uint16_t addr) {
    stack_push(regPC_higher);
    stack_push(regPC_lower);
    regPC = addr;
}
static int cpu_exec_op(uint8_t opcode) {
    int operation_cycles = -1;
    // print_op(regPC, opcode);
    switch (opcode) {
        case 0x00: // NOP; 1 byte; 4 cycles
            operation_cycles = 4;
            break;
        case 0x01: // LXI B,D16; 3 bytes; 10 cycles
            regC = get_next_prog_byte();
            regB = get_next_prog_byte();
            operation_cycles = 10;
            break;
        case 0x02: // STAX B; 1 byte; 7 cycles
            memory_store(regBC, regA);
            operation_cycles = 7;
            break;
        case 0x03: // INX B; 1 byte; 5 cycles
            regBC = (regBC + 1) & 0xFFFF;
            operation_cycles = 5;
            break;
        case 0x04: // INR B; 1 byte; 5 cycles; Z,S,P,AC flags
            regB = inc8bit_with_flags(regB);
            operation_cycles = 5;
            break;
        case 0x05: // DCR B; 1 byte; 5 cycles; Z,S,P,AC flags
            regB = dec8bit_with_flags(regB);
            operation_cycles = 5;
            break;
        case 0x06: // MVI B,D8; 2 bytes; 7 cycles
            regB = get_next_prog_byte();
            operation_cycles = 7;
            break;
        case 0x07: // RLC; 1 byte; 4 cycles; C flag
            status_reg.flags.C = ((regA & 0x80) != 0);
            regA = (regA << 1) | status_reg.flags.C;
            operation_cycles = 4;
            break;
        case 0x08: // -; 1 byte; 4 cycles
            operation_cycles = 4;
            break;
        case 0x09: // DAD B; 1 byte; 10 cycles; C flag
            regHL = add16bit_with_flag(regHL, regBC);
            operation_cycles = 10;
            break;
        case 0x0A: // LDAX B; 1 byte; 7 cycles
            regA = memory_get(regBC);
            operation_cycles = 7;
            break;
        case 0x0B: // DCX B; 1 byte; 5 cycles
            regBC = (regBC - 1) & 0xFFFF;
            operation_cycles = 5;
            break;
        case 0x0C: // INR C; 1 byte; 5 cycles; Z,S,P,AC flags
            regC = inc8bit_with_flags(regC);
            operation_cycles = 5;
            break;
        case 0x0D: // DCR C; 1 byte; 5 cycles; Z,S,P,AC flags
            regC = dec8bit_with_flags(regC);
            operation_cycles = 5;
            break;
        case 0x0E: // MVI C,D8; 2 bytes; 7 cycles
            regC = get_next_prog_byte();
            operation_cycles = 7;
            break;
        case 0x0F: // RRC; 1 byte; 4 cycles; C flag
            status_reg.flags.C = (regA & 0x01);
            regA = (regA >> 1) | (status_reg.flags.C << 7);
            operation_cycles = 4;
            break;
        case 0x10: // -; 1 byte; 4 cycles
            operation_cycles = 4;
            break;
        case 0x11: // LXI D,D16; 3 bytes; 10 cycles
            regE = get_next_prog_byte();
            regD = get_next_prog_byte();
            operation_cycles = 10;
            break;
        case 0x12: // STAX D; 1 byte; 7 cycles
            memory_store(regDE, regA);
            operation_cycles = 7;
            break;
        case 0x13: // INX D; 1 byte; 5 cycles
            regDE = (regDE + 1) & 0xFFFF;
            operation_cycles = 5;
            break;
        case 0x14: // INR D; 1 byte; 5 cycles; Z,S,P,AC flags
            regD = inc8bit_with_flags(regD);
            operation_cycles = 5;
            break;
        case 0x15: // DCR D; 1 byte; 5 cycles; Z,S,P,AC flags
            regD = dec8bit_with_flags(regD);
            operation_cycles = 5;
            break;
        case 0x16: // MVI D,D8; 2 bytes; 7 cycles
            regD = get_next_prog_byte();
            operation_cycles = 7;
            break;
        case 0x17: // RAL; 1 byte; 4 cycles; C flag
            {
                uint8_t old_C_flag = status_reg.flags.C;
                status_reg.flags.C = ((regA & 0x80) != 0);
                regA = (regA << 1) | old_C_flag;
            }
            operation_cycles = 4;
            break;
        case 0x18: // -; 1 byte; 4 cycles
            operation_cycles = 4;
            break;
        case 0x19: // DAD D; 1 byte; 10 cycles; C flag
            regHL = add16bit_with_flag(regHL, regDE);
            operation_cycles = 10;
            break;
        case 0x1A: // LDAX D; 1 byte; 7 cycles
            regA = memory_get(regDE);
            operation_cycles = 7;
            break;
        case 0x1B: // DCX D; 1 byte; 5 cycles
            regDE = (regDE - 1) & 0xFFFF;
            operation_cycles = 5;
            break;
        case 0x1C: // INR E; 1 byte; 5 cycles; Z,S,P,AC flags
            regE = inc8bit_with_flags(regE);
            operation_cycles = 5;
            break;
        case 0x1D: // DCR E; 1 byte; 5 cycles; Z,S,P,AC flags
            regE = dec8bit_with_flags(regE);
            operation_cycles = 5;
            break;
        case 0x1E: // MVI E,D8; 2 bytes; 7 cycles
            regE = get_next_prog_byte();
            operation_cycles = 7;
            break;
        case 0x1F: // RAR; 1 byte; 4 cycles; C flag
            {
                uint8_t old_C_flag = status_reg.flags.C;
                status_reg.flags.C = (regA & 0x01);
                regA = (regA >> 1) | (old_C_flag << 7);
            }
            operation_cycles = 4;
            break;
        case 0x20: // -; 1 byte; 4 cycles
            operation_cycles = 4;
            break;
        case 0x21: // LXI H,D16; 3 bytes; 10 cycles
            regL = get_next_prog_byte();
            regH = get_next_prog_byte();
            operation_cycles = 10;
            break;
        case 0x22: // SHLD adr; 3 bytes; 16 cycles
            {
                uint16_t addr = get_next_2_prog_bytes();
                memory_store(addr, regL);
                memory_store(addr+1, regH);
            }
            operation_cycles = 16;
            break;
        case 0x23: // INX H; 1 byte; 5 cycles
            regHL = (regHL + 1) & 0xFFFF;
            operation_cycles = 5;
            break;
        case 0x24: // INR H; 1 byte; 5 cycles; Z,S,P,AC flags
            regH = inc8bit_with_flags(regH);
            operation_cycles = 5;
            break;
        case 0x25: // DCR H; 1 byte; 5 cycles; Z,S,P,AC flags
            regH = dec8bit_with_flags(regH);
            operation_cycles = 5;
            break;
        case 0x26: // MVI H,D8; 2 bytes; 7 cycles
            regH = get_next_prog_byte();
            operation_cycles = 7;
            break;
        case 0x27: // DAA; 1 byte; 4 cycles
            /*
            * The behavoir of flags was developed to pass all the tests I had
            * since all the documentation I could find was a bit lacking on this topic
            */
            if ((regA & 0x0F) > 9 || status_reg.flags.AC) {
                // The C flag should be set to (regA + 0x06) > 0x100, so (regA > 0xA0)
                status_reg.flags.C |= (regA > 0xA0);
                // The AC flag would be set to 1 if (regA & 0x0F) + 6 > 15, so (regA & 0x0F) > 9
                status_reg.flags.AC = ((regA & 0x0F) > 0x09);
                regA = (regA + 0x06) & 0xFF;
            } else {
                status_reg.flags.AC = 0;
            }
            if ((regA >> 4) > 9 || status_reg.flags.C) {
                status_reg.flags.C  = 1;
                regA = (regA + 0x60) & 0xFF;
            }
            calc_set_P_flag(regA);
            calc_set_S_flag(regA);
            calc_set_Z_flag(regA);
            operation_cycles = 4;
            break;
        case 0x28: // -; 1 byte; 4 cycles
            operation_cycles = 4;
            break;
        case 0x29: // DAD H; 1 byte; 10 cycles; C flag
            regHL = add16bit_with_flag(regHL, regHL);
            operation_cycles = 10;
            break;
        case 0x2A: // LHLD adr; 3 bytes; 16 cycles
            {
                uint16_t addr = get_next_2_prog_bytes();
                regL = memory_get(addr);
                regH = memory_get(addr+1);
            }
            operation_cycles = 16;
            break;
        case 0x2B: // DCX H; 1 byte; 5 cycles
            regHL = (regHL - 1) & 0xFFFF;
            operation_cycles = 5;
            break;
        case 0x2C: // INR L; 1 byte; 5 cycles; Z,S,P,AC flags
            regL = inc8bit_with_flags(regL);
            operation_cycles = 5;
            break;
        case 0x2D: // DCR L; 1 byte; 5 cycles; Z,S,P,AC flags
            regL = dec8bit_with_flags(regL);
            operation_cycles = 5;
            break;
        case 0x2E: // MVI L,D8; 2 bytes; 7 cycles
            regL = get_next_prog_byte();
            operation_cycles = 7;
            break;
        case 0x2F: // CMA; 1 byte; 4 cycles
            regA = ~regA;
            operation_cycles = 4;
            break;
        case 0x30: // -; 1 byte; 4 cycles
            operation_cycles = 4;
            break;
        case 0x31: // LXI SP,D16; 3 bytes; 10 cycles
            regSP_lower = get_next_prog_byte();
            regSP_higher = get_next_prog_byte();
            operation_cycles = 10;
            break;
        case 0x32: // STA adr; 3 bytes; 13 cycles
            memory_store(get_next_2_prog_bytes(), regA);
            operation_cycles = 13;
            break;
        case 0x33: // INX SP; 1 byte; 5 cycles
            regSP = (regSP + 1) & 0xFFFF;
            operation_cycles = 5;
            break;
        case 0x34: // INR M; 1 byte; 10 cycles; Z,S,P,AC flags
            memory_store(regHL, inc8bit_with_flags(memory_get(regHL)));
            operation_cycles = 10;
            break;
        case 0x35: // DCR M; 1 byte; 10 cycles; Z,S,P,AC flags
            memory_store(regHL, dec8bit_with_flags(memory_get(regHL)));
            operation_cycles = 10;
            break;
        case 0x36: // MVI M,D8; 2 bytes; 10 cycles
            memory_store(regHL, get_next_prog_byte());
            operation_cycles = 10;
            break;
        case 0x37: // STC; 1 byte; 4 cycles; C flag
            status_reg.flags.C = 1;
            operation_cycles = 4;
            break;
        case 0x38: // -; 1 byte; 4 cycles
            operation_cycles = 4;
            break;
        case 0x39: // DAD SP; 1 byte; 10 cycles; C flag
            regHL = add16bit_with_flag(regHL, regSP);
            operation_cycles = 10;
            break;
        case 0x3A: // LDA adr; 3 bytes; 13 cycles
            {
                uint8_t lower = get_next_prog_byte();
                uint8_t higher = get_next_prog_byte();
                regA = memory_get(join_bytes(higher, lower));
            }
            operation_cycles = 13;
            break;
        case 0x3B: // DCX SP; 1 byte; 5 cycles
            regSP = (regSP - 1) & 0xFFFF;
            operation_cycles = 5;
            break;
        case 0x3C: // INR A; 1 byte; 5 cycles; Z,S,P,AC flags
            regA = inc8bit_with_flags(regA);
            operation_cycles = 5;
            break;
        case 0x3D: // DCR A; 1 byte; 5 cycles; Z,S,P,AC flags
            regA = dec8bit_with_flags(regA);
            operation_cycles = 5;
            break;
        case 0x3E: // MVI A,D8; 2 bytes; 7 cycles
            regA = get_next_prog_byte();
            operation_cycles = 7;
            break;
        case 0x3F: // CMC; 1 byte; 4 cycles
            status_reg.flags.C = ~status_reg.flags.C;
            operation_cycles = 4;
            break;
        case 0x40: // MOV B,B; 1 byte; 5 cycles
            operation_cycles = 5;
            break;
        case 0x41: // MOV B,C; 1 byte; 5 cycles
            regB = regC;
            operation_cycles = 5;
            break;
        case 0x42: // MOV B,D; 1 byte; 5 cycles
            regB = regD;
            operation_cycles = 5;
            break;
        case 0x43: // MOV B,E; 1 byte; 5 cycles
            regB = regE;
            operation_cycles = 5;
            break;
        case 0x44: // MOV B,H; 1 byte; 5 cycles
            regB = regH;
            operation_cycles = 5;
            break;
        case 0x45: // MOV B,L; 1 byte; 5 cycles
            regB = regL;
            operation_cycles = 5;
            break;
        case 0x46: // MOV B,M; 1 byte; 7 cycles
            regB = memory_get(regHL);
            operation_cycles = 7;
            break;
        case 0x47: // MOV B,A; 1 byte; 5 cycles
            regB = regA;
            operation_cycles = 5;
            break;
        case 0x48: // MOV C,B; 1 byte; 5 cycles
            regC = regB;
            operation_cycles = 5;
            break;
        case 0x49: // MOV C,C; 1 byte; 5 cycles
            operation_cycles = 5;
            break;
        case 0x4A: // MOV C,D; 1 byte; 5 cycles
            regC = regD;
            operation_cycles = 5;
            break;
        case 0x4B: // MOV C,E; 1 byte; 5 cycles
            regC = regE;
            operation_cycles = 5;
            break;
        case 0x4C: // MOV C,H; 1 byte; 5 cycles
            regC = regH;
            operation_cycles = 5;
            break;
        case 0x4D: // MOV C,L; 1 byte; 5 cycles
            regC = regL;
            operation_cycles = 5;
            break;
        case 0x4E: // MOV C,M; 1 byte; 7 cycles
            regC = memory_get(regHL);
            operation_cycles = 7;
            break;
        case 0x4F: // MOV C,A; 1 byte; 5 cycles
            regC = regA;
            operation_cycles = 5;
            break;
        case 0x50: // MOV D,B; 1 byte; 5 cycles
            regD = regB;
            operation_cycles = 5;
            break;
        case 0x51: // MOV D,C; 1 byte; 5 cycles
            regD = regC;
            operation_cycles = 5;
            break;
        case 0x52: // MOV D,D; 1 byte; 5 cycles
            operation_cycles = 5;
            break;
        case 0x53: // MOV D,E; 1 byte; 5 cycles
            regD = regE;
            operation_cycles = 5;
            break;
        case 0x54: // MOV D,H; 1 byte; 5 cycles
            regD = regH;
            operation_cycles = 5;
            break;
        case 0x55: // MOV D,L; 1 byte; 5 cycles
            regD = regL;
            operation_cycles = 5;
            break;
        case 0x56: // MOV D,M; 1 byte; 7 cycles
            regD = memory_get(regHL);
            operation_cycles = 7;
            break;
        case 0x57: // MOV D,A; 1 byte; 5 cycles
            regD = regA;
            operation_cycles = 5;
            break;
        case 0x58: // MOV E,B; 1 byte; 5 cycles
            regE = regB;
            operation_cycles = 5;
            break;
        case 0x59: // MOV E,C; 1 byte; 5 cycles
            regE = regC;
            operation_cycles = 5;
            break;
        case 0x5A: // MOV E,D; 1 byte; 5 cycles
            regE = regD;
            operation_cycles = 5;
            break;
        case 0x5B: // MOV E,E; 1 byte; 5 cycles
            operation_cycles = 5;
            break;
        case 0x5C: // MOV E,H; 1 byte; 5 cycles
            regE = regH;
            operation_cycles = 5;
            break;
        case 0x5D: // MOV E,L; 1 byte; 5 cycles
            regE = regL;
            operation_cycles = 5;
            break;
        case 0x5E: // MOV E,M; 1 byte; 7 cycles
            regE = memory_get(regHL);
            operation_cycles = 7;
            break;
        case 0x5F: // MOV E,A; 1 byte; 5 cycles
            regE = regA;
            operation_cycles = 5;
            break;
        case 0x60: // MOV H,B; 1 byte; 5 cycles
            regH = regB;
            operation_cycles = 5;
            break;
        case 0x61: // MOV H,C; 1 byte; 5 cycles
            regH = regC;
            operation_cycles = 5;
            break;
        case 0x62: // MOV H,D; 1 byte; 5 cycles
            regH = regD;
            operation_cycles = 5;
            break;
        case 0x63: // MOV H,E; 1 byte; 5 cycles
            regH = regE;
            operation_cycles = 5;
            break;
        case 0x64: // MOV H,H; 1 byte; 5 cycles
            operation_cycles = 5;
            break;
        case 0x65: // MOV H,L; 1 byte; 5 cycles
            regH = regL;
            operation_cycles = 5;
            break;
        case 0x66: // MOV H,M; 1 byte; 7 cycles
            regH = memory_get(regHL);
            operation_cycles = 7;
            break;
        case 0x67: // MOV H,A; 1 byte; 5 cycles
            regH = regA;
            operation_cycles = 5;
            break;
        case 0x68: // MOV L,B; 1 byte; 5 cycles
            regL = regB;
            operation_cycles = 5;
            break;
        case 0x69: // MOV L,C; 1 byte; 5 cycles
            regL = regC;
            operation_cycles = 5;
            break;
        case 0x6A: // MOV L,D; 1 byte; 5 cycles
            regL = regD;
            operation_cycles = 5;
            break;
        case 0x6B: // MOV L,E; 1 byte; 5 cycles
            regL = regE;
            operation_cycles = 5;
            break;
        case 0x6C: // MOV L,H; 1 byte; 5 cycles
            regL = regH;
            operation_cycles = 5;
            break;
        case 0x6D: // MOV L,L; 1 byte; 5 cycles
            operation_cycles = 5;
            break;
        case 0x6E: // MOV L,M; 1 byte; 7 cycles
            regL = memory_get(regHL);
            operation_cycles = 7;
            break;
        case 0x6F: // MOV L,A; 1 byte; 5 cycles
            regL = regA;
            operation_cycles = 5;
            break;
        case 0x70: // MOV M,B; 1 byte; 7 cycles
            memory_store(regHL, regB);
            operation_cycles = 7;
            break;
        case 0x71: // MOV M,C; 1 byte; 7 cycles
            memory_store(regHL, regC);
            operation_cycles = 7;
            break;
        case 0x72: // MOV M,D; 1 byte; 7 cycles
            memory_store(regHL, regD);
            operation_cycles = 7;
            break;
        case 0x73: // MOV M,E; 1 byte; 7 cycles
            memory_store(regHL, regE);
            operation_cycles = 7;
            break;
        case 0x74: // MOV M,H; 1 byte; 7 cycles
            memory_store(regHL, regH);
            operation_cycles = 7;
            break;
        case 0x75: // MOV M,L; 1 byte; 7 cycles
            memory_store(regHL, regL);
            operation_cycles = 7;
            break;
        case 0x76: // HLT; 1 byte; 7 cycles
            cpu_state.halted = true;
            operation_cycles = 7;
            break;
        case 0x77: // MOV M,A; 1 byte; 7 cycles
            memory_store(regHL, regA);
            operation_cycles = 7;
            break;
        case 0x78: // MOV A,B; 1 byte; 5 cycles
            regA = regB;
            operation_cycles = 5;
            break;
        case 0x79: // MOV A,C; 1 byte; 5 cycles
            regA = regC;
            operation_cycles = 5;
            break;
        case 0x7A: // MOV A,D; 1 byte; 5 cycles
            regA = regD;
            operation_cycles = 5;
            break;
        case 0x7B: // MOV A,E; 1 byte; 5 cycles
            regA = regE;
            operation_cycles = 5;
            break;
        case 0x7C: // MOV A,H; 1 byte; 5 cycles
            regA = regH;
            operation_cycles = 5;
            break;
        case 0x7D: // MOV A,L; 1 byte; 5 cycles
            regA = regL;
            operation_cycles = 5;
            break;
        case 0x7E: // MOV A,M; 1 byte; 7 cycles
            regA = memory_get(regHL);
            operation_cycles = 7;
            break;
        case 0x7F: // MOV A,A; 1 byte; 5 cycles
            operation_cycles = 5;
            break;
        case 0x80: // ADD B; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regB, 0);
            operation_cycles = 4;
            break;
        case 0x81: // ADD C; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regC, 0);
            operation_cycles = 4;
            break;
        case 0x82: // ADD D; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regD, 0);
            operation_cycles = 4;
            break;
        case 0x83: // ADD E; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regE, 0);
            operation_cycles = 4;
            break;
        case 0x84: // ADD H; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regH, 0);
            operation_cycles = 4;
            break;
        case 0x85: // ADD L; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regL, 0);
            operation_cycles = 4;
            break;
        case 0x86: // ADD M; 1 byte; 7 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, memory_get(regHL), 0);
            operation_cycles = 7;
            break;
        case 0x87: // ADD A; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regA, 0);
            operation_cycles = 4;
            break;
        case 0x88: // ADC B; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regB, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x89: // ADC C; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regC, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x8A: // ADC D; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regD, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x8B: // ADC E; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regE, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x8C: // ADC H; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regH, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x8D: // ADC L; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regL, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x8E: // ADC M; 1 byte; 7 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, memory_get(regHL), status_reg.flags.C);
            operation_cycles = 7;
            break;
        case 0x8F: // ADC A; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, regA, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x90: // SUB B; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regB, 0);
            operation_cycles = 4;
            break;
        case 0x91: // SUB C; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regC, 0);
            operation_cycles = 4;
            break;
        case 0x92: // SUB D; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regD, 0);
            operation_cycles = 4;
            break;
        case 0x93: // SUB E; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regE, 0);
            operation_cycles = 4;
            break;
        case 0x94: // SUB H; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regH, 0);
            operation_cycles = 4;
            break;
        case 0x95: // SUB L; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regL, 0);
            operation_cycles = 4;
            break;
        case 0x96: // SUB M; 1 byte; 7 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, memory_get(regHL), 0);
            operation_cycles = 7;
            break;
        case 0x97: // SUB A; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regA, 0);
            operation_cycles = 4;
            break;
        case 0x98: // SBB B; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regB, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x99: // SBB C; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regC, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x9A: // SBB D; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regD, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x9B: // SBB E; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regE, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x9C: // DBB H; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regH, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x9D: // SBB L; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regL, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0x9E: // SBB M; 1 byte; 7 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, memory_get(regHL), status_reg.flags.C);
            operation_cycles = 7;
            break;
        case 0x9F: // SBB A; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, regA, status_reg.flags.C);
            operation_cycles = 4;
            break;
        case 0xA0: // ANA B; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, regB);
            operation_cycles = 4;
            break;
        case 0xA1: // ANA C; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, regC);
            operation_cycles = 4;
            break;
        case 0xA2: // ANA D; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, regD);
            operation_cycles = 4;
            break;
        case 0xA3: // ANA E; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, regE);
            operation_cycles = 4;
            break;
        case 0xA4: // ANA H; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, regH);
            operation_cycles = 4;
            break;
        case 0xA5: // ANA L; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, regL);
            operation_cycles = 4;
            break;
        case 0xA6: // ANA M; 1 byte; 7 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, memory_get(regHL));
            operation_cycles = 7;
            break;
        case 0xA7: // ANA A; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, regA);
            operation_cycles = 4;
            break;
        case 0xA8: // XRA B; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, regB);
            operation_cycles = 4;
            break;
        case 0xA9: // XRA C; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, regC);
            operation_cycles = 4;
            break;
        case 0xAA: // XRA D; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, regD);
            operation_cycles = 4;
            break;
        case 0xAB: // XRA E; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, regE);
            operation_cycles = 4;
            break;
        case 0xAC: // XRA H; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, regH);
            operation_cycles = 4;
            break;
        case 0xAD: // XRA L; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, regL);
            operation_cycles = 4;
            break;
        case 0xAE: // XRA M; 1 byte; 7 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, memory_get(regHL));
            operation_cycles = 7;
            break;
        case 0xAF: // XRA A; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, regA);
            operation_cycles = 4;
            break;
        case 0xB0: // ORA B; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, regB);
            operation_cycles = 4;
            break;
        case 0xB1: // ORA C; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, regC);
            operation_cycles = 4;
            break;
        case 0xB2: // ORA D; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, regD);
            operation_cycles = 4;
            break;
        case 0xB3: // ORA E; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, regE);
            operation_cycles = 4;
            break;
        case 0xB4: // ORA H; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, regH);
            operation_cycles = 4;
            break;
        case 0xB5: // ORA L; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, regL);
            operation_cycles = 4;
            break;
        case 0xB6: // ORA M; 1 byte; 7 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, memory_get(regHL));
            operation_cycles = 7;
            break;
        case 0xB7: // ORA A; 1 byte; 4 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, regA);
            operation_cycles = 4;
            break;
        case 0xB8: // CMP B; 1 byte; 4 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, regB, 0);
            operation_cycles = 4;
            break;
        case 0xB9: // CMP C; 1 byte; 4 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, regC, 0);
            operation_cycles = 4;
            break;
        case 0xBA: // CMP D; 1 byte; 4 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, regD, 0);
            operation_cycles = 4;
            break;
        case 0xBB: // CMP E; 1 byte; 4 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, regE, 0);
            operation_cycles = 4;
            break;
        case 0xBC: // CMP H; 1 byte; 4 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, regH, 0);
            operation_cycles = 4;
            break;
        case 0xBD: // CMP L; 1 byte; 4 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, regL, 0);
            operation_cycles = 4;
            break;
        case 0xBE: // CMP M; 1 byte; 7 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, memory_get(regHL), 0);
            operation_cycles = 4;
            break;
        case 0xBF: // CMP A; 1 byte; 4 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, regA, 0);
            operation_cycles = 4;
            break;
        case 0xC0: // RNZ; 1 byte; 11/5 cycles
            operation_cycles = cond_return(!status_reg.flags.Z);
            break;
        case 0xC1: // POP B; 1 byte; 10 cycles
            regC = stack_pop();
            regB = stack_pop();
            operation_cycles = 10;
            break;
        case 0xC2: // JNZ adr; 3 bytes; 10 cycles
            cond_jump(!status_reg.flags.Z);
            operation_cycles = 10;
            break;
        case 0xC3: // JMP adr; 3 bytes; 10 cycles
            regPC = get_next_2_prog_bytes();
            operation_cycles = 10;
            break;
        case 0xC4: // CNZ adr; 3 bytes; 17/11 cycles
            operation_cycles = cond_call(!status_reg.flags.Z);
            break;
        case 0xC5: // PUSH B; 1 byte; 11 cycles
            stack_push(regB);
            stack_push(regC);
            operation_cycles = 11;
            break;
        case 0xC6: // ADI D8; 2 bytes; 7 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, get_next_prog_byte(), 0);
            operation_cycles = 7;
            break;
        case 0xC7: // RST 0; 1 byte; 11 cycles
            call_addr(0x0000);
            operation_cycles = 11;
            break;
        case 0xC8: // RZ; 1 byte; 11/5 cycles
            operation_cycles = cond_return(status_reg.flags.Z);
            break;
        case 0xC9: // RET; 1 byte; 10 cycles
            regPC_lower = stack_pop();
            regPC_higher = stack_pop();
            operation_cycles = 10;
            break;
        case 0xCA: // JZ adr; 3 bytes; 10 cycles
            cond_jump(status_reg.flags.Z);
            operation_cycles = 10;
            break;
        case 0xCB: // - (works as JMP addr); 3 bytes; 10 cycles
            regPC = get_next_2_prog_bytes();
            operation_cycles = 10;
            break;
        case 0xCC: // CZ adr; 3 bytes; 17/11 cycles
            operation_cycles = cond_call(status_reg.flags.Z);
            break;
        case 0xCD: // CALL adr; 3 bytes; 17 cycles
            {
                uint16_t newPC = get_next_2_prog_bytes();
                stack_push(regPC_higher);
                stack_push(regPC_lower);
                regPC = newPC;
            }
            operation_cycles = 17;
            break;
        case 0xCE: // ACI D8; 2 bytes; 7 cycles; Z,S,P,C,AC flags
            regA = add8bit_with_flags(regA, get_next_prog_byte(), status_reg.flags.C);
            operation_cycles = 7;
            break;
        case 0xCF: // RST 1; 1 byte; 11 cycles
            call_addr(0x0008);
            operation_cycles = 11;
            break;
        case 0xD0: // RNC; 1 byte; 11/5 cycles
            operation_cycles = cond_return(!status_reg.flags.C);
            break;
        case 0xD1: // POP D; 1 byte; 10 cycles
            regE = stack_pop();
            regD = stack_pop();
            operation_cycles = 10;
            break;
        case 0xD2: // JNC adr; 3 bytes; 10 cycles
            cond_jump(!status_reg.flags.C);
            operation_cycles = 10;
            break;
        case 0xD3: // OUT D8; 2 bytes; 10 cycles
            io_write(get_next_prog_byte(), regA);
            operation_cycles = 10;
            break;
        case 0xD4: // CNC adr; 3 bytes; 17/11 cycles
            operation_cycles = cond_call(!status_reg.flags.C);
            break;
        case 0xD5: // PUSH D; 1 byte; 11 cycles
            stack_push(regD);
            stack_push(regE);
            operation_cycles = 11;
            break;
        case 0xD6: // SUI D8; 2 bytes; 7 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, get_next_prog_byte(), 0);
            operation_cycles = 7;
            break;
        case 0xD7: // RST 2; 1 byte; 11 cycles
            call_addr(0x0010);
            operation_cycles = 11;
            break;
        case 0xD8: // RC; 1 byte; 11/5 cycles
            operation_cycles = cond_return(status_reg.flags.C);
            break;
        case 0xD9: // - (works as RET); 1 byte; 10 cycles
            regPC_lower = stack_pop();
            regPC_higher = stack_pop();
            operation_cycles = 10;
            break;
        case 0xDA: // JC adr; 3 bytes; 10 cycles
            cond_jump(status_reg.flags.C);
            operation_cycles = 10;
            break;
        case 0xDB: // IN D8; 2 bytes; 10 cycles
            regA = io_read(get_next_prog_byte());
            operation_cycles = 10;
            break;
        case 0xDC: // CC adr; 3 bytes; 17/11 cycles
            operation_cycles = cond_call(status_reg.flags.C);
            break;
        case 0xDD: // - (works as CALL addr); 3 bytes; 17 cycles
            {
                uint16_t newPC = get_next_2_prog_bytes();
                stack_push(regPC_higher);
                stack_push(regPC_lower);
                regPC = newPC;
            }
            operation_cycles = 17;
            break;
        case 0xDE: // SBI D8; 2 bytes; 7 cycles; Z,S,P,C,AC flags
            regA = sub8bit_with_flags(regA, get_next_prog_byte(), status_reg.flags.C);
            operation_cycles = 7;
            break;
        case 0xDF: // RST 3; 1 byte; 11 cycles
            call_addr(0x0018);
            operation_cycles = 11;
            break;
        case 0xE0: // RPO; 1 byte; 11/5 cycles
            operation_cycles = cond_return(!status_reg.flags.P);
            break;
        case 0xE1: // POP H; 1 byte; 10 cycles
            regL = stack_pop();
            regH = stack_pop();
            operation_cycles = 10;
            break;
        case 0xE2: // JPO adr; 3 bytes; 10 cycles
            cond_jump(!status_reg.flags.P);
            operation_cycles = 10;
            break;
        case 0xE3: // XTHL; 1 byte; 18 cycles
            {
                uint8_t tmp = regL;
                regL = memory_get(regSP);
                memory_store(regSP, tmp);
                tmp = regH;
                regH = memory_get(regSP+1);
                memory_store(regSP+1, tmp);
            }
            operation_cycles = 18;
            break;
        case 0xE4: // CPO adr; 3 bytes; 17/11 cycles
            operation_cycles = cond_call(!status_reg.flags.P);
            break;
        case 0xE5: // PUSH H; 1 byte; 11 cycles
            stack_push(regH);
            stack_push(regL);
            operation_cycles = 11;
            break;
        case 0xE6: // ANI D8; 2 bytes; 7 cycles; Z,S,P,C,AC flags
            regA = and8bit_with_flags(regA, get_next_prog_byte());
            operation_cycles = 7;
            break;
        case 0xE7: // RST 4; 1 byte; 11 cycles
            call_addr(0x0020);
            operation_cycles = 11;
            break;
        case 0xE8: // RPE; 1 byte; 11/5 cycles
            operation_cycles = cond_return(status_reg.flags.P);
            break;
        case 0xE9: // PCHL; 1 byte; 5 cycles
            regPC = regHL;
            operation_cycles = 5;
            break;
        case 0xEA: // JPE adr; 3 bytes; 10 cycles
            cond_jump(status_reg.flags.P);
            operation_cycles = 10;
            break;
        case 0xEB: // XCHG; 1 byte; 5 cycles
            {
                uint16_t tmp = regHL;
                regHL = regDE;
                regDE = tmp;
            }
            operation_cycles = 5;
            break;
        case 0xEC: // CPE adr; 3 bytes; 17/11 cycles
            operation_cycles = cond_call(status_reg.flags.P);
            break;
        case 0xED: // - (works as CALL addr); 3 bytes; 17 cycles
            {
                uint16_t newPC = get_next_2_prog_bytes();
                stack_push(regPC_higher);
                stack_push(regPC_lower);
                regPC = newPC;
            }
            operation_cycles = 17;
            break;
        case 0xEE: // XRI D8; 2 bytes; 7 cycles; Z,S,P,C,AC flags
            regA = xor8bit_with_flags(regA, get_next_prog_byte());
            operation_cycles = 7;
            break;
        case 0xEF: // RST 5; 1 byte; 11 cycles
            call_addr(0x0028);
            operation_cycles = 11;
            break;
        case 0xF0: // RP; 1 byte; 11/5 cycles
            operation_cycles = cond_return(!status_reg.flags.S); // If the number is positive
            break;
        case 0xF1: // POP PSW; 1 byte; 10 cycles
            status_reg.single = stack_pop();
            regA = stack_pop();
            status_reg.flags._unused1 = 1;
            status_reg.flags._unused2 = 0;
            status_reg.flags._unused3 = 0;
            operation_cycles = 10;
            break;
        case 0xF2: // JP adr; 3 bytes; 10 cycles
            cond_jump(!status_reg.flags.S); // If the number is positive
            operation_cycles = 10;
            break;
        case 0xF3: // DI; 1 byte; 4 cycles
            cpu_state.interrupts_enabled = false;
            operation_cycles = 4;
            break;
        case 0xF4: // CP adr; 3 bytes; 17/11 cycles
            operation_cycles = cond_call(!status_reg.flags.S); // If the number is positive
            break;
        case 0xF5: // PUSH PSW; 1 byte; 11 cycles
            stack_push(regA);
            stack_push(status_reg.single);
            operation_cycles = 11;
            break;
        case 0xF6: // ORI D8; 2 bytes; 7 cycles; Z,S,P,C,AC flags
            regA = or8bit_with_flags(regA, get_next_prog_byte());
            operation_cycles = 7;
            break;
        case 0xF7: // RST 6; 1 byte; 11 cycles
            call_addr(0x0030);
            operation_cycles = 11;
            break;
        case 0xF8: // RM; 1 byte; 11/5 cycles
            operation_cycles = cond_return(status_reg.flags.S); // If the number is negative
            break;
        case 0xF9: // SPHL; 1 byte; 5 cycles
            regSP = regHL;
            operation_cycles = 5;
            break;
        case 0xFA: // JM adr; 3 bytes; 10 cycles
            cond_jump(status_reg.flags.S); // If the number is negative
            operation_cycles = 10;
            break;
        case 0xFB: // EI; 1 byte; 4 cycles
            cpu_state.interrupts_enabled = true;
            operation_cycles = 4;
            break;
        case 0xFC: // CM adr; 3 bytes; 17/11 cycles
            operation_cycles = cond_call(status_reg.flags.S); // If the number is negative
            break;
        case 0xFD: // - (works as CALL addr); 3 bytes; 17 cycles
            {
                uint16_t newPC = get_next_2_prog_bytes();
                stack_push(regPC_higher);
                stack_push(regPC_lower);
                regPC = newPC;
            }
            operation_cycles = 17;
            break;
        case 0xFE: // CPI D8; 2 bytes; 7 cycles; Z,S,P,C,AC flags
            sub8bit_with_flags(regA, get_next_prog_byte(), 0);
            operation_cycles = 7;
            break;
        case 0xFF: // RST 7; 1 byte; 11 cycles
            call_addr(0x0038);
            operation_cycles = 11;
            break;
        default:
            break;
    }
    if (operation_cycles == -1) {
        printf("Operation not implemented: %02X\n", opcode);
    }
    return operation_cycles;
}


void cpu_init() {
    regPC = 0;
    regSP = 0;
    regA = 0;
    regBC = 0;
    regDE = 0;
    regHL = 0;
    status_reg.single = 0x02;
    cpu_state.halted = false;
    cpu_state.interrupts_enabled = false;
    cpu_state.requested_interrupt_opcode = 0;
}

int cpu_step() {
    if (!cpu_state.halted) {
        if (cpu_state.interrupts_enabled && cpu_state.requested_interrupt_opcode) {
            cpu_exec_op(cpu_state.requested_interrupt_opcode);
            cpu_state.requested_interrupt_opcode = 0;
        } else {
            return cpu_exec_op(get_next_prog_byte());
        }
    } else {
        printf("HALTED\n");
        return 4;
    }
}

void cpu_set_PC_reg(uint16_t val) {
    regPC = val;
}

uint16_t cpu_get_PC_reg() {
    return regPC;
}

uint8_t cpu_get_C_reg() {
    return regC;
}

uint8_t cpu_get_E_reg() {
    return regE;
}

uint16_t cpu_get_DE_reg() {
    return regDE;
}
