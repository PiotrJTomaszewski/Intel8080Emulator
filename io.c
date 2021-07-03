#include <stdio.h>
#include "io.h"

void io_write(uint8_t dev_id, uint8_t data) {
    printf("Device %02X: %02X\n", dev_id, data);
}

uint8_t io_read(uint8_t dev_id) {
    return 0;
}
