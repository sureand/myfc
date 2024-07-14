#ifndef __PPU_HEADER
#define __PPU_HEADER
#include "common.h"

BYTE ppu_read(PPU* ppu, uint16_t addr);
void ppu_write(PPU* ppu, uint16_t addr, uint8_t value);
#endif