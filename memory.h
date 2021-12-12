#ifndef __MEMORY_HEADER
#define __MEMORY_HEADER
#include "common.h"

void mem_init(ROM *rom);
void write_byte(WORD address, BYTE data);
BYTE read_byte(WORD address);
WORD read_word(WORD address);
void write_word(WORD address, WORD data);

#endif