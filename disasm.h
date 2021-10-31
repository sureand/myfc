#ifndef __FC_DISASM__
#define __FC_DISASM__
#include "common.h"


BYTE do_read_byte(WORD address, BYTE *buf);
void display(BYTE *data, size_t count);
#endif