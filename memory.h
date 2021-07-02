#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>


void memory_store(uint16_t address, uint8_t value);

uint8_t memory_get(uint16_t address);

#endif // __MEMORY_H__