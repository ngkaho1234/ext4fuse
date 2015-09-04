#ifndef EXT4_CRC_H
#define EXT4_CRC_H

#include "types/ext4_basic.h"

uint16_t ext4_crc16(uint16_t crc, const void *buf, int len);
uint32_t ext4_crc32c(uint32_t crc, const void *buf, int size);

#endif

