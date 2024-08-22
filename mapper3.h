#ifndef __MAPPER3_HEADER
#define __MAPPER3_HEADER
#include "common.h"

BYTE prg_rom_read3(WORD address);
void prg_rom_write3(WORD address, BYTE data);
BYTE chr_rom_read3(WORD address);
void chr_rom_write3(WORD address, BYTE data);
void mapper_reset3();

#endif