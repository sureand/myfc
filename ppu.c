
#include "ppu.h"

void ppu_init(PPU* ppu)
{
    memset(ppu, 0, sizeof(PPU));
}

BYTE ppu_read(PPU* ppu, uint16_t addr)
{
    BYTE data = 0;

    switch (addr & 0x2007) {

        case 0x2002:
            // 返回PPU状态寄存器的值，并清除VBlank标志位
            data = ppu->ppustatus;
            ppu->ppustatus &= 0x7F; // 清除VBlank标志位（bit7）
            break;
        case 0x2004:
            data = ppu->oamdata[ppu->oamaddr];
            ppu->oamaddr = (ppu->oamaddr + 1) % OAM_SIZE; // 循环OAM地址
            break;
        case 0x2007:
            // 从PPU数据寄存器读取数据，并更新PPU地址寄存器的地址
            data = ppu->vram[ppu->ppuaddr];
            if (ppu->ppuctrl & 0x04) {
                ppu->ppuaddr += 32; // 垂直增量模式
            } else {
                ppu->ppuaddr += 1;  // 水平增量模式
            }
            ppu->ppuaddr &= 0x3FFF; // 确保地址在VRAM范围内
            break;
        default:
            printf("incomplete register:[0x%X]!\n", addr);
            break;
    }

    return data;
}

void ppu_write(PPU* ppu, uint16_t addr, uint8_t value)
{
    switch (addr & 0x2007) { // 只关注地址的低3位，因为高位是重复的
        case 0x2000: // PPUCTRL
            ppu->ppuctrl = value;
            break;
        case 0x2001: // PPUMASK
            ppu->ppumask = value;
            break;
        case 0x2003: // OAMADDR
            ppu->oamaddr = value;
            break;
        case 0x2004: // OAMDATA
            ppu->oamdata[ppu->oamaddr] = value;
            ppu->oamaddr = (ppu->oamaddr + 1) % OAM_SIZE; // 循环OAM地址
            break;
        case 0x2005: // PPUSCROLL
            if (ppu->write_latch == 0) {
                ppu->scroll_x = value;
                ppu->write_latch = 1;
            } else {
                ppu->scroll_y = value;
                ppu->write_latch = 0;
            }
            break;
        case 0x2006: // PPUADDR
            if (ppu->write_latch == 0) {
                ppu->ppuaddr = (ppu->ppuaddr & 0x00FF) | (value << 8); // 设置高8位地址
                ppu->write_latch = 1;
            } else {
                ppu->ppuaddr = (ppu->ppuaddr & 0xFF00) | value; // 设置低8位地址
                ppu->write_latch = 0;
            }
            break;
        case 0x2007: // PPUDATA
            ppu->vram[ppu->ppuaddr] = value;
            if (ppu->ppuctrl & 0x04) {
                ppu->ppuaddr += 32; // 垂直增量模式
            } else {
                ppu->ppuaddr += 1;  // 水平增量模式
            }
            ppu->ppuaddr &= 0x3FFF; // 确保地址在VRAM范围内
            break;
        case 0x4014: // OAMDMA
            printf("incomplete register:[0x%X]!\n", addr);
            break;
        default:
            // 忽略未定义的写入
            break;
    }
}
