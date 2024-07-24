#ifndef __CPU__HEADER__
#define __CPU__HEADER__
#include "common.h"

#define OP_LEN (8)
typedef struct
{
    char op_len;
    char *op_name;
    char cycle;

    //对应代码的执行函数
    void (*op_func)(BYTE);
}INS;

INS code_maps[0X100];

BYTE cpu_read_byte(WORD address);
void cpu_write_byte(WORD address, BYTE data);
WORD cpu_read_word(WORD address);
void cpu_write_word(WORD address, WORD data);

#define op(c, s, n, p, func) \
    code_maps[0x##c].op_name = s; \
    code_maps[0x##c].op_len = n; \
    code_maps[0x##c].cycle = p; \
    code_maps[0x##c].op_func = func; \

#endif