#ifndef __FC_DISASM__
#define __FC_DISASM__
#include "common.h"

void display(BYTE *data, size_t count);
void disassemble();
void parse_code();
#endif