#ifndef __MAPPER_HEADER
#define __MAPPER_HEADER
#include "common.h"
#include "mapper0.h"
#include "mapper1.h"
#include "mapper2.h"
#include "mapper3.h"
#include "mapper4.h"

extern MAPPER mappers[0x100];

#define CREATE_MAPPER(n, s) \
    mappers[n].number = n; \
    mappers[n].name = s; \
    mappers[n].prg_rom_read = prg_rom_read##n; \
    mappers[n].prg_rom_write = prg_rom_write##n; \
    mappers[n].chr_rom_read = chr_rom_read##n; \
    mappers[n].chr_rom_write = chr_rom_write##n; \
    mappers[n].irq_scanline = irq_scanline##n; \
    mappers[n].mapper_reset = mapper_reset##n; \

#endif