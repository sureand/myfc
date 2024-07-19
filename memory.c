#include "memory.h"
#include "cpu.h"
#include "ppu.h"

BYTE bus_read(WORD address)
{
    /* 由于 0x0800 - 0x1FFF 是CPU RAM 的镜像, 直接取模*/
    if (address <= 0x1FFF) {
        return cpu_read_byte(address & 0x7FF);
    }

    /* ppu 的处理 */
    if (address >= 0x2000 && address <= 0x3FFF) {
        return ppu_read(address & 0x2007);
    }

    /* 手柄处理 */
    if (address >= 0x4000 && address <= 0x401F) {
        printf("incomplete interface!")
        ;
    }

    /* Expansion ROM (可选，用于扩展硬件) */
    if (address >= 0x4020 && address <= 0x5FFF) {
        return rom_read(address);
    }

    /* SRAM: 0x6000-0x7FFF(用于保存记录) */
    if (address >= 0x6000 && address <= 0x7FFF) {
        return sram[address - 0x6000];
    }

    /*PRG ROM 和 CHR ROM 主程序*/
    if (address >= 0x8000 && address <= 0xFFFF) {
        address -= 0x8000;
        return rom_read(address);
    }

    printf("Read from unsupported address: %04X\n", address);
    _exit(-1);

    return 0;
}

void bus_write(WORD address, BYTE data)
{
    if (address < 0x1FFF) {
        address %= CPU_RAM_SIZE;
        return cpu_write_byte(address & 0x7FF, data);
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        return ppu_write(address & 0x2007, data);
    }

    /* 手柄处理 */
    if (address >= 0x4000 && address <= 0x401F) {
        printf("incomplete interface!")
        ;
    }

    /* Expansion ROM (可选，用于扩展硬件) */
    if (address >= 0x4020 && address <= 0x5FFF) {
        return rom_write(address, data);
    }

    /* SRAM: 0x6000-0x7FFF(用于保存记录) */
    if (address >= 0x6000 && address <= 0x7FFF) {
        return sram[address - 0x6000] = data;
    }

    /*PRG ROM 和 CHR ROM 主程序*/
    if (address >= 0x8000 && address <= 0xFFFF) {
        return prg_rom[address - 0x8000] = data;
    }
}