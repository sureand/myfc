#include "mapper0.h"

static inline WORD get_address(WORD address)
{
    WORD address_range = (get_current_rom()->header->prg_rom_count > 1) ? 0x7FFF : 0x3FFF;
    return (address & address_range);
}

BYTE prg_rom_read0(WORD address)
{
    WORD _address = get_address(address);

    return get_current_rom()->prg_rom[_address];
}

void prg_rom_write0(WORD address, BYTE data)
{
    WORD _address = get_address(address);
    (void)_address;
}

BYTE chr_rom_read0(WORD address)
{
    if (get_current_rom()->header->chr_rom_count == 0) {
        return ppu.vram[address];
    }

    return get_current_rom()->chr_rom[address];
}

void chr_rom_write0(WORD address, BYTE data)
{
    if (get_current_rom()->header->chr_rom_count == 0) {
        ppu.vram[address] = data;
    }
}

void irq_scanline0()
{

}

void mapper_reset0()
{
}