#include "mapper.h"
#include "mapper0.h"
#include "mapper1.h"
#include "mapper2.h"
#include "mapper3.h"

/*
    经过查阅资料, 访问 CPU 地址 0x8000 - 0xFFFF 实际是通过硬件实现映射到PRG ROM 芯片的
    因此mapper 的实现, 其实是通过PRG ROM 芯片或者 CHR ROM 芯片做的映射.
*/

static inline MAPPER *get_mapper()
{
    BYTE number = get_current_rom()->header->mapper_number;
    MAPPER *mapper = &mappers[number];

    return mapper;
}

void mapper_init()
{
    memset(mappers, 0, sizeof(mappers));

    CREATE_MAPPER(0, "NROM");
    CREATE_MAPPER(1, "SxROM");
    CREATE_MAPPER(2, "UXROM");
    CREATE_MAPPER(3, "MMC1");

    get_mapper()->mapper_reset();
}

BYTE prg_rom_read(WORD address)
{
    return get_mapper()->prg_rom_read(address);
}

void prg_rom_write(WORD address, BYTE data)
{
    get_mapper()->prg_rom_write(address, data);
}

BYTE chr_rom_read(WORD address)
{
    return get_mapper()->chr_rom_read(address);
}

void chr_rom_write(WORD address, BYTE data)
{
    get_mapper()->chr_rom_write(address, data);
}

void mapper_reset()
{
    get_mapper()->mapper_reset();
}

void irq_scanline()
{
    get_mapper()->irq_scanline();
}