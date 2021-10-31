#ifndef __FC_COMMON_HEADER__
#define __FC_COMMON_HEADER__
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifndef NULL
#define NULL (void*)0
#endif

#ifndef BYTE
#define BYTE uint8_t
#endif

#ifndef WORD
#define WORD uint16_t
#endif

//ROM 相关
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

//内存相关
typedef struct
{
    BYTE RAM[0x600];
    BYTE ZERO_PAGE[0x100];
    BYTE STACK[0x100];
    BYTE MIRROR[0x1800];
    BYTE IO_Register[0x8];
    BYTE PPU_DISRegister[0x8];
    BYTE SRAM[0x2000];
    BYTE EXPAND_ROM[0x1FE0];
    BYTE *PROG_ROM_LOWER;
    BYTE *PROG_ROM_UPPER;

}_MEM;


static _MEM *global_mem;

//CPU 相关
/*
资料来自: http://nesbbs.com/bbs/thread-1024-1-1.html
7 6 5 4 3 2 1 0
N V E B D I Z C
*/
/*
N:负数标志，指令指行完后为负(>7F)则为1,否则为0
V:溢出标志，若产生溢出则V=1,否则V=0
B:Break指令，用于判别中断是由BRK指令引起还是实际的中断引起
D:十进制模式标志，为1表示十进制模式，为0表示16进制模式。
（一旦进入十进制模式ADC和SBC两条指令会把操作数当作10进制数，不推荐使用）
I:中断禁用标志，是否允许系统中断IRQ,=1:禁止，=0:允许
Z:零标志，结果是否为0,为0则Z=1,否则Z=0
C:进位标志,结果最高位有进位则C=1,否则C=0
*/

#define NEG   (1 << 7)
#define OF    (1 << 6)
#define EX    (1 << 5)
#define BRK   (1 << 4)
#define DEC   (1 << 3)
#define INT   (1 << 2)
#define ZERO  (1 << 1)
#define CARRY (1 << 0)

typedef struct {
    WORD IP; //指令寄存器
    BYTE SP; //栈指针
    BYTE A; //累加寄存器
    BYTE X; //变址寄存器
    BYTE Y; //变址寄存器

    //状态寄存器
    BYTE P;

    BYTE reversed; //保留, 用来作为结构体对齐使用

}_CPU;

#define INLINE_VOID inline void

static _CPU cpu;

#define PC (cpu.IP)

#endif