#include "mapper3.h"

static BYTE bank_number;

static inline WORD get_address(WORD address)
{
    WORD address_range = (get_current_rom()->header->prg_rom_count > 1) ? 0x7FFF : 0x3FFF;
    return (address & address_range);
}

BYTE prg_rom_read3(WORD address)
{
    WORD _address = get_address(address);
    return get_current_rom()->prg_rom[_address];
}

void prg_rom_write3(WORD address, BYTE data)
{
    bank_number = data & 0x3;
}

static inline size_t get_chr_address(WORD address)
{
    return bank_number * 0x2000 + address;
}

BYTE chr_rom_read3(WORD address)
{
    size_t _address = get_chr_address(address);
    return get_current_rom()->chr_rom[_address];
}

void chr_rom_write3(WORD address, BYTE data)
{
    (void)address;
    (void)data;
}

void mapper_reset3()
{
    bank_number = 0;
}


