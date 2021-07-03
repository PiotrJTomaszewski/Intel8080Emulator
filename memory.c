#include "memory.h"

static uint16_t data[MEM_SIZE];

void memory_store(uint16_t address, uint8_t value) {
    data[address] = value;
}

uint8_t memory_get(uint16_t address) {
    return data[address];
}
