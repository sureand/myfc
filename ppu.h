#ifndef __PPU_HEADER
#define __PPU_HEADER
#include "common.h"
#include "SDL2/SDL.h"

BYTE ppu_read(WORD addr);
void ppu_write(WORD addr, uint8_t data);
void ppu_vram_write(WORD address, BYTE data);

#endif