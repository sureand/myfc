#ifndef __MAPPER2_HEADER
#define __MAPPER2_HEADER
#include "common.h"

BYTE prg_rom_read2(WORD address);
void prg_rom_write2(WORD address, BYTE data);
BYTE chr_rom_read2(WORD address);
void chr_rom_write2(WORD address, BYTE data);
void irq_scanline2();
void mapper_reset2();

#endif