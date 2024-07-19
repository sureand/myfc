
#include "ppu.h"

void ppu_init()
{
    memset(&ppu, 0, sizeof(_PPU));
}

BYTE ppu_read(WORD address)
{
    BYTE data = 0;

    if (address < 0x2000) {
        return ppu.vram[address];
    }

    switch (address) {

        case 0x2002:
            // 返回PPU状态寄存器的值，并清除VBlank标志位
            data = ppu.ppustatus;
            ppu.ppustatus &= 0x7F; // 清除VBlank标志位（bit7）
            ppu.write_latch = 0; // Reset toggle
            break;
        case 0x2004:
            data = ppu.oamdata[ppu.oamaddr];
            break;
        case 0x2007:
            // 缓冲区读取逻辑
            if (ppu.ppuaddr < 0x3F00) {
                data = ppu.vram_buffer;
                ppu.vram_buffer = ppu.vram[ppu.ppuaddr];
            } else {
                // 直接读取调色板数据，无需缓冲
                data = ppu.vram[ppu.ppuaddr];
                ppu.vram_buffer = ppu.vram[ppu.ppuaddr - 0x1000];
            }

            // 更新PPU地址
            if (ppu.ppuctrl & 0x04) {
                ppu.ppuaddr += 32; // 垂直增量模式
            } else {
                ppu.ppuaddr += 1;  // 水平增量模式
            }
            ppu.ppuaddr &= 0x3FFF; // 确保地址在VRAM范围内
            break;
        default:
            fprintf(stderr, "incomplete register:[0x%X]!\n", address);
            break;
    }

    return data;
}

void ppu_write(WORD address, uint8_t data)
{
    if (address < 0x2000) {
       ppu.vram[address] = data;
       return;
    }

    switch (address) { // 使用低3位，因为高位是重复的
        case 0x2000: // PPUCTRL
            ppu.ppuctrl = data;
            break;
        case 0x2001: // PPUMASK
            ppu.ppumask = data;
            break;
        case 0x2003: // OAMADDR
            ppu.oamaddr = data;
            break;
        case 0x2004: // OAMDATA
            ppu.oamdata[ppu.oamaddr] = data;
            ppu.oamaddr = (ppu.oamaddr + 1) & 0xFF; // 循环OAM地址
            break;
        case 0x2005: // PPUSCROLL
            if (ppu.write_latch == 0) {
                ppu.scroll_x = data;
                ppu.write_latch = 1;
            } else {
                ppu.scroll_y = data;
                ppu.write_latch = 0;
            }
            break;
        case 0x2006: // PPUADDR
            if (ppu.write_latch == 0) {
                ppu.ppuaddr = (ppu.ppuaddr & 0x00FF) | (data << 8); // 设置高8位地址
                ppu.write_latch = 1;
            } else {
                ppu.ppuaddr = (ppu.ppuaddr & 0xFF00) | data; // 设置低8位地址
                ppu.write_latch = 0;
            }
            break;
        case 0x2007: // PPUDATA
            ppu.vram[ppu.ppuaddr] = data;
            if (ppu.ppuctrl & 0x04) {
                ppu.ppuaddr += 32; // 垂直增量模式
            } else {
                ppu.ppuaddr += 1;  // 水平增量模式
            }
            ppu.ppuaddr &= 0x3FFF; // 确保地址在VRAM范围内
            break;
        case 0x4014: // OAMDMA
            {
                WORD address = data << 8;
                for (int i = 0; i < OAM_SIZE; i++) {
                    ppu.oamdata[i] = bus_read(address + i); // bus_read 函数需要访问 CPU 内存
                }
            }
            break;
        default:
            fprintf(stderr, "Write to unsupported PPU register: [0x%X]!\n", address);
            // 忽略未定义的写入
            break;
    }
}

