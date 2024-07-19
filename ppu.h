#ifndef __PPU_HEADER
#define __PPU_HEADER
#include "common.h"

BYTE ppu_read(WORD addr);
void ppu_write(WORD addr, uint8_t data);
#endif