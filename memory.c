#include <stdio.h>
#include <stdlib.h>
#include "memory.h"

static uint8_t memory_data[MEMORY_SIZE];

void memory_read_file(char *path, uint16_t start_at) {
    // TODO: Rework this, also handle file opening errors
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("Program open error");
        exit(-1);
    }
    fread(memory_data+start_at, 1, (MEMORY_SIZE - start_at), file);
    fclose(file);
}

void memory_store(uint16_t address, uint8_t value) {
    memory_data[address] = value;
    // printf("WRITE: %04X, VAL: %02X\n", address, memory_data[address]);
}

uint8_t memory_get(uint16_t address) {
    // printf("READ: %04X, VAL: %02X\n", address, memory_data[address]);
    return memory_data[address];
}
