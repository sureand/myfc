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
    void (*op_func)();
}INS;

INS code_maps[0X100];

void init_code();

void init_cpu();
void show_code(ROM *rom);

#endif