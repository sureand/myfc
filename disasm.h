#ifndef __FC_DISASM__
#define __FC_DISASM__
#include "common.h"

#define OP_LEN (8)
typedef struct
{
    char op_len;
    char *op_name;
    char cycle;
}INS;

INS code_maps[0X100];

void init_code();
BYTE do_read_byte(WORD address, BYTE *buf);
void display(BYTE *data, size_t count);
#endif