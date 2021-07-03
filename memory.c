#include <stdio.h>
#include "memory.h"

static uint8_t memory_data[MEMORY_SIZE];

void memory_read_file(char *path) {
    FILE *file = fopen(path, "r");
    fread(memory_data, 1, MEMORY_SIZE, file);
    fclose(file);
}

void memory_store(uint16_t address, uint8_t value) {
    memory_data[address] = value;
}

uint8_t memory_get(uint16_t address) {
    return memory_data[address];
}
