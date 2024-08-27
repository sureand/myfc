#ifndef __MAPPER4_HEADER
#define __MAPPER4_HEADER
#include "common.h"


typedef struct {
    BYTE target_register;
    BYTE R[8];
    BYTE prg_mode;
    BYTE prg_bank_number;
    BYTE chr_bank_number;
    BYTE chr_inversion;
    BYTE write_protect;

    BYTE irq_enable;
    BYTE irq_latch;
    BYTE irq_reload;
    WORD irq_counter;
}MMC4_REGISTER;

BYTE prg_rom_read4(WORD address);
void prg_rom_write4(WORD address, BYTE data);
BYTE chr_rom_read4(WORD address);
void chr_rom_write4(WORD address, BYTE data);
void irq_scanline4();
void mapper_reset4();

#endif