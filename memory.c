#include "memory.h"

static BYTE *mem_addr(WORD address, _MEM *mem)
{
    assert(address >= 0);

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
    if(address >= 0x8000 && address < 0xC000) {
        address -= 0x8000;
        return &mem->PROG_ROM_LOWER[address];
    }

    //PRG UPPER
    if(address >= 0xC000) {
        address -= 0xC000;
        return &mem->PROG_ROM_UPPER[address];
    }

    fprintf(stderr, "un expected address:%04X\n", address);
    exit(-1);

    return NULL;
}

static inline BYTE do_mem_read(WORD address, _MEM *mem)
{
    BYTE *addr = mem_addr(address, mem);
    return *addr;
}

static inline BYTE mem_read(WORD address)
{
    return do_mem_read(address, global_mem);
}

static inline void do_mem_write(WORD address, BYTE data)
{
    BYTE *addr = mem_addr(address, global_mem);
    *addr = data;
}

static inline void mem_write(WORD address, BYTE data)
{
    do_mem_write(address, data);
}

void write_byte(WORD address, BYTE data)
{
    mem_write(address, data);
}

void write_word(WORD address, WORD data)
{
    BYTE *bt = (BYTE*)&data;

    //小端 低字节在前、高字节在后
    mem_write(address, bt[1]);
    mem_write(address + 1, bt[0]);
}

BYTE read_byte(WORD address)
{
    return mem_read(address);
}

WORD read_word(WORD address)
{
    BYTE bt1 = read_byte(address);
    BYTE bt2 = read_byte(address + 1);

    WORD bt = (bt2 << 1) | bt1;

    return bt;
}

void mem_init(_MEM *mem)
{
    assert(mem);

    global_mem = mem;
}