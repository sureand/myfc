#include "common.h"

//这里是一些全局使用的变量

ROM *current_rom;

BYTE mirroring;

/* 是否使用 PRG RAM*/
BYTE use_prg_ram;

/* 电池空间, 8k*/
BYTE sram[SRAM_SIZE];

/* 拓展rom */
BYTE extend_rom[0x2000]; // 一般的拓展rom 是8k 左右

/* 窗口标题 */
char window_title[256];

/* SDL 窗口指针 */
SDL_Window *current_window;

uint8_t rom_loaded;

_CPU cpu;
_PPU ppu;
APU apu;
