#ifndef __IO_H__
#define __IO_H__

#include <stdint.h>

void io_write(uint8_t dev_id, uint8_t data);

uint8_t io_read(uint8_t dev_id);

#endif // __IO_H__
