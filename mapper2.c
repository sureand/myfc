#include "mapper2.h"

static WORD bank_select;

static inline size_t get_address(WORD address)
{
    if (address < 0xC000) {
        return bank_select * PRG_ROM_PAGE_SIZE + ((address - 0x8000) & 0x3FFF);
    }

    size_t last_bank_addr = (get_current_rom()->header->prg_rom_count - 1) * PRG_ROM_PAGE_SIZE;

    return last_bank_addr + (address & 0x3FFF);
}

BYTE prg_rom_read2(WORD address)
{
    size_t _address = get_address(address);
    BYTE data = get_current_rom()->prg_rom[_address];

    return data;
}

void prg_rom_write2(WORD address, BYTE data)
{
    (void)address;

	bank_select = data & 0xF;
}

BYTE chr_rom_read2(WORD address)
{
    if (get_current_rom()->header->chr_rom_count == 0) {
        return ppu.vram[address];
    }

    return get_current_rom()->chr_rom[address];
}

void chr_rom_write2(WORD address, BYTE data)
{
    if (get_current_rom()->header->chr_rom_count == 0) {
        ppu.vram[address] = data;
    }
}

void mapper_reset2()
{
    bank_select = 0;
}
