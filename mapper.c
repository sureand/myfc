#include "mapper.h"

MAPPER mappers[0x100];

static MAPPER *active_mapper = NULL;

static inline MAPPER *get_mapper_for_current_rom()
{
    BYTE number = get_current_rom()->header->mapper_number;
    MAPPER *mapper = &mappers[number];

    if (!mapper->prg_rom_read || !mapper->prg_rom_write ||
        !mapper->chr_rom_read || !mapper->chr_rom_write ||
        !mapper->irq_scanline || !mapper->mapper_reset) {
        fprintf(stderr, "ERROR, mapper: %d is not support!\n", number);
        exit(-1);
    }

    return mapper;
}

static inline MAPPER *get_active_mapper()
{
    if (!active_mapper) {
        active_mapper = get_mapper_for_current_rom();
    }

    return active_mapper;
}

void mapper_init()
{
    memset(mappers, 0, sizeof(mappers));

    CREATE_MAPPER(0, "NROM");
    CREATE_MAPPER(1, "SxROM");
    CREATE_MAPPER(2, "UXROM");
    CREATE_MAPPER(3, "MMC1");
    CREATE_MAPPER(4, "MMC3");

    active_mapper = get_mapper_for_current_rom();
    active_mapper->mapper_reset();
}

BYTE prg_rom_read(WORD address)
{
    return get_active_mapper()->prg_rom_read(address);
}

void prg_rom_write(WORD address, BYTE data)
{
    get_active_mapper()->prg_rom_write(address, data);
}

BYTE chr_rom_read(WORD address)
{
    return get_active_mapper()->chr_rom_read(address);
}

void chr_rom_write(WORD address, BYTE data)
{
    get_active_mapper()->chr_rom_write(address, data);
}

void mapper_reset()
{
    active_mapper = get_mapper_for_current_rom();
    active_mapper->mapper_reset();
}

void irq_scanline()
{
    get_active_mapper()->irq_scanline();
}
