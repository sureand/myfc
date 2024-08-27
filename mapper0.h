#ifndef __MAPPER0_HEADER
#define __MAPPER0_HEADER
#include "common.h"

BYTE prg_rom_read0(WORD address);
void prg_rom_write0(WORD address, BYTE data);
BYTE chr_rom_read0(WORD address);
void chr_rom_write0(WORD address, BYTE data);
void irq_scanline0();
void mapper_reset0();

#endif
