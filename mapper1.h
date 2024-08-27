#ifndef __MAPPER1_HEADER
#define __MAPPER1_HEADER
#include "common.h"

typedef struct {

    BYTE shift_reg;
    BYTE prg_mode;
    BYTE chr_mode;
    WORD prg_bank_number;
    BYTE chr_bank_number1;
    BYTE chr_bank_number2;
    BYTE write_count;
}MMC1_REGISTER;

BYTE prg_rom_read1(WORD address);
void prg_rom_write1(WORD address, BYTE data);
BYTE chr_rom_read1(WORD address);
void chr_rom_write1(WORD address, BYTE data);
void irq_scanline1();
void mapper_reset1();

#endif