#include "memory.h"

static BYTE *mem_addr(WORD address, _MEM *mem)
{
    assert(address >= 0 && address <= 0x10000);

    //zero page
    if(address <= 0xFF) return &mem->ZERO_PAGE[address];

    //stack
    if(address >= 0x100 && address <= 0x1FF) {
        address -= 0x100;
        return &mem->STACK[address];
    }

    //RAM
    if(address >= 0x200 && address <= 0x7FF) {
        address -= 0x200;
        return &mem->RAM[address];
    }

    //mirror
    if(address >= 0x800 && address <= 0x2000) {
        address -= 0x800;
        address &= 0x07FF;
        return &mem->MIRROR[address];
    }

    //io register
    if(address >= 0x2000 && address <= 0x401F) {
        address -= 0x2000;
        return &mem->IO_Register[address];
    }

    //Expansion ROM
    if(address >= 0x4020 && address <= 0x5FFF) {
        address -= 0x4020;
        return &mem->IO_Register[address];
    }

    //SRAM
    if(address >= 0x6000 && address <= 0x7FFF) {
        address -= 0x6000;
        return &mem->SRAM[address];
    }

    //PRG LOWER
    if(address >= 0x8000 && address <= 0xC000) {
        address -= 0x8000;
        return &mem->PROG_ROM_LOWER[address];
    }

    //PRG UPPER
    if(address >= 0xC000 && address <= 0x10000) {
        address -= 0xC000;
        return &mem->PROG_ROM_UPPER[address];
    }

    fprintf(stderr, "un expected address:%u\n", address);
    exit(-1);

    return NULL;
}

BYTE mem_read(WORD address, _MEM *mem)
{
    BYTE *addr = mem_addr(address, mem);
    return *addr;
}

void mem_write(WORD address, _MEM *mem, BYTE data)
{
    BYTE *addr = mem_addr(address, mem);
    *addr = data;
}