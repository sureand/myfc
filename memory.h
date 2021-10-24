#ifndef __MEMORY_HEADER
#define __MEMORY_HEADER
#include "common.h"
typedef struct
{
    BYTE RAM[0x600];
    BYTE ZERO_PAGE[0x100];
    BYTE STACK[0x100];
    BYTE MIRROR[0x1800];
    BYTE IO_Register[0x8];
    BYTE PPU_DISRegister[0x8];
    BYTE SRAM[0x2000];
    BYTE EXPAND_ROM[0x1FE0];
    BYTE *PROG_ROM_LOWER;
    BYTE *PROG_ROM_UPPER;

}_MEM;

BYTE mem_read(WORD address, _MEM *mem);
void mem_write(WORD address, _MEM *mem, BYTE data);

#endif