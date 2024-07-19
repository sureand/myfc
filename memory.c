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

    /* APU寄存器，用于控制音频通道（脉冲波、三角波、噪声、DMC） */
    if (address >= 0x4000 && address <= 0x4013) {
        printf("incomplete interface!");
        //TODO: apu_read(address, data); // 需要实现的函数
    }

    /* APU状态寄存器，用于启用或禁用音频通道，并读取APU的状态。*/
    if (address = 0x4015) {
        printf("incomplete interface!");
        //TODO: apu_status_read(address); // 需要实现的函数
    }

    /* 手柄处理 */
    if (address >= 0x4016 && address <= 0x4017) {
        printf("incomplete interface!");
        //TODO: controller_read(address);
    }

    /* Expansion ROM (可选，用于扩展硬件) */
    if (address >= 0x4020 && address <= 0x5FFF) {
        return extend_rom[address - 0x4020];
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
        return cpu_write_byte(address & 0x7FF, data);
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        return ppu_write(address & 0x2007, data);
    }

    /* APU 的读写 */
    if (address >= 0x4000 && address <= 0x4013) {
        printf("incomplete interface!");
        //TODO: apu_write(address, data); // 需要实现的函数
    }

    /* OAM DMA 写入 */
    if (address == 0x4014) {
        WORD dma_address = data << 8;
        for (int i = 0; i < 256; i++) {
            ppu.oamdata[i] = cpu_read_byte(dma_address + i); // bus_read 函数需要访问 CPU 内存
        }
        return;
    }

    /* APU状态寄存器，用于启用或禁用音频通道，并读取APU的状态。*/
    if (address = 0x4015) {
        printf("incomplete interface!");
        //apu_status_write(data); // 需要实现的函数
    }

    /* 手柄处理 */
    if (address >= 0x4016 && address <= 0x4017) {
        printf("incomplete interface!");
        //TODO: controller_write(address);
    }

    /* Expansion ROM (可选，用于扩展硬件) */
    if (address >= 0x4020 && address <= 0x5FFF) {
        extend_rom[address - 0x4020] = data;
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