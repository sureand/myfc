#ifndef __HEADER__LOAD_ROM__
#define __HEADER__LOAD_ROM__
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define HEADER_LEN (16)

typedef struct
{
    //nes 版本
    char version;

    //显示器相关flag, wiki 没有命名
    BYTE flag1;

    //机器相关flag
    BYTE flag2;

    //程序只读存储器个数, 以4K 为单位
    BYTE prg_rom_count;

    //角色只读存储器个数, 以2K 为单位
    BYTE chr_rom_count;

    //是否包含Trainer区域
    BYTE type;

    //保留, 默认都是0
    BYTE reserved[8];
}ROM_HEADER;


typedef struct 
{
    ROM_HEADER *header;

    BYTE *body;
    BYTE *prg_rom;
    BYTE *chr_rom;
}ROM;

ROM *make_rom();
void FREE(void *p);
ROM_HEADER *parse_header(const FILE *fp);
ROM *parse_rom(const FILE *fp);
ROM *load_rom(const char *path);
int load_data(const char *path);

#endif