#ifndef __FC_COMMON_HEADER__
#define __FC_COMMON_HEADER__
#include <stdio.h>
#include <string.h>
#include <stdint.h>
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

/*NES地址空间布局
1. Zero Page: 0x0000-0x00FF(256 bytes)
2. Stack: 0x0100-0x01FF(256 bytes)
3. RAM: Ox0200-0x07FF(1.5 KB)
4. Mirroring: 0x0800-0x1FFF(CPU RAM的镜像，每2KB重复)
---上述都是CPU RAM
5.PPU Registers: 0x2000-0x3FFF(PPU寄存器，每8字节重复)
6. APU and l/0 Registers: 0x4000-0x401F
7. Joypad and l/0 Ports: 0x4016-0x4017
8.Expansion ROM: 0x4020-0x5FFF(可选，用于扩展硬件)
9.SRAM: 0x6000-0x7FFF(Battery-backed Save RAM)
10.PRG-ROM Lower Bank: 0x8000 -0xBFFF
11.PRG-ROM Upper Bank: 0xC000 -0xFFFF
*/

#define CPU_RAM_SIZE 0x0800  // 2KB RAM
#define PPU_REG_SIZE 0x0008  // 8 PPU寄存器
#define APU_REG_SIZE 0x0020  // 32 APU寄存器
#define OAM_SIZE     0x0100  // 256字节 OAM
#define VRAM_SIZE    0x4000  // 16KB VRAM
#define SRAM_SIZE    0x8000  // 32KB PRG ROM
#define PRG_ROM_SIZE 0x8000  // 32KB PRG ROM

// NES内存映射地址
#define CPU_RAM_START 0x0000
#define CPU_RAM_END   0x1FFF
#define PPU_REG_START 0x2000
#define PPU_REG_END   0x3FFF
#define APU_REG_START 0x4000
#define APU_REG_END   0x401F
#define JOYPAD_START  0x4016
#define JOYPAD_END    0x4017
#define PRG_ROM_START 0x8000
#define PRG_ROM_END   0xFFFF

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
    BYTE cpu_ram[0x800];
    BYTE ppu_reg[0x008];
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

/* PRG ROM 32k, 程序的rom 内容*/
BYTE prg_rom[PRG_ROM_SIZE];

/* 电池空间, 8k*/
BYTE sram[SRAM_SIZE];


_MEM *global_mem;

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

#define NEG   (7)
#define OF    (6)
#define EX    (5)
#define BRK   (4)
#define DEC   (3)
#define INT   (2)
#define ZERO  (1)
#define CARRY (0)

typedef struct {

    WORD IP; //指令寄存器
    BYTE SP; //栈指针
    BYTE A; //累加寄存器
    BYTE X; //变址寄存器
    BYTE Y; //变址寄存器

    //状态寄存器
    BYTE P;

    BYTE reversed; //保留, 用来作为结构体对齐使用

    size_t cycle;

    BYTE ram[CPU_RAM_SIZE];  // 2KB RAM
}_CPU;

#define VRAM_SIZE   0x4000 // VRAM大小为16KB
#define OAM_SIZE    0x100  // OAM大小为256字节
#define APU_REG_SIZE 0x20 //APU 的大小

typedef struct {

    uint8_t ppuctrl;    // PPU控制寄存器 ($2000)
    uint8_t ppumask;    // PPU掩码寄存器 ($2001)
    uint8_t ppustatus;  // PPU状态寄存器 ($2002)，读取时会更新
    uint8_t oamaddr;    // OAM地址寄存器 ($2003)
    uint8_t oamdata[OAM_SIZE]; // OAM数据数组 ($2004)
    uint8_t ppuscroll;  // 滚动值，用于计算当前显示的tile
    uint16_t ppuaddr;   // PPU地址寄存器 ($2006/$2007)，用于VRAM访问
    uint8_t ppudat;     // PPU数据寄存器，用于VRAM读写的临时存储
    uint8_t vram[VRAM_SIZE]; // VRAM内存数组，模拟NES的图形存储
    uint8_t oamdma;     // OAM DMA寄存器 ($4014)，用于CPU到OAM的DMA传输
    uint8_t cycle;      // 当前的PPU周期计数
    uint8_t scanline;   // 当前的扫描线计数
    uint8_t scroll_x;
    uint8_t scroll_y;
    uint8_t write_latch; // 用于跟踪第一次和第二次写入

    // 以下寄存器用于模拟PPU内部渲染逻辑
    uint8_t fineX;      // 用于渲染时的微分X位置
    uint16_t vram_tmp;  // 临时VRAM地址，用于内部计算
    uint8_t vram_buffer;// 用于存储从VRAM读取的数据
    uint8_t *nametable; // 指向当前名称表的指针
    uint8_t *pattern;   // 指向当前使用模式的指针（背景或精灵）
    // 其他可能的内部状态，如渲染状态机、精灵评估逻辑等
} _PPU;

// APU结构体
typedef struct {
    uint8_t registers[APU_REG_SIZE];  // APU寄存器
} APU;

// 手柄结构体
typedef struct {
    uint8_t state;
} _Joypad;

// NES总线结构体，包含各个组件
typedef struct {
    _CPU cpu;
    _PPU ppu;
    APU apu;
    _Joypad joypad;
} NES;

void ppu_init();

//详情看 https://www.nesdev.org/wiki/PPU_registers

#define INLINE_VOID inline void

_CPU cpu;
_PPU ppu;

#define PC (cpu.IP)

BYTE bus_read(WORD address);
void bus_write(WORD address, BYTE data);

#endif