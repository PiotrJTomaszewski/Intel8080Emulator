#include <stdio.h>
#include "io.h"

void io_write(uint8_t dev_id, uint8_t data) {
    printf("Device 0x%02X write: 0x%02X\n", dev_id, data);
}

uint8_t io_read(uint8_t dev_id) {
    printf("Device 0x%02X read\n", dev_id);
    return 0;
}
