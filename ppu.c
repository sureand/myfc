
#include "ppu.h"

/*

$0000-$0FFF: Pattern Table 0
$1000-$1FFF: Pattern Table 1
$2000-$23BF: Name Table 0
$23C0-$23FF: Attribute Table 0
$2400-$27BF: Name Table 1
$27C0-$27FF: Attribute Table 1
$2800-$2BBF: Name Table 2 (镜像或实际存在)
$2BC0-$2BFF: Attribute Table 2
$2C00-$2FBF: Name Table 3 (镜像或实际存在)
$2FC0-$2FFF: Attribute Table 3
$3000-$3EFF: 镜像地址
$3F00-$3F0F: Background Palette
$3F10-$3F1F: Sprite Palette
$3F20-$3FFF: 调色板镜像
*/

void ppu_init()
{
    memset(&ppu, 0, sizeof(_PPU));
}

// 读取 VRAM
BYTE ppu_vram_read(WORD address)
{
    address &= 0x3FFF;

    if (address < 0x2000) {
        return ppu.vram[address];
    } else if (address >= 0x2000 && address < 0x3F00) {
        return ppu.vram[address & 0x2FFF];
    }

    return ppu.vram[address & 0x3F1F];
}

// 写入 VRAM
void ppu_vram_write(WORD address, BYTE data)
{
    address &= 0x3FFF;

    if (address < 0x2000) {
        ppu.vram[address] = data;
    } else if (address >= 0x2000 && address < 0x3F00) {
        ppu.vram[address & 0x2FFF] = data;
    }

    ppu.vram[address & 0x3F1F] = data;
}

BYTE ppu_read(WORD address)
{
    BYTE data = 0;

    switch (address) {

        //返回ppu mask 的状态
        case 0x2001:
            data = ppu.ppumask;
            break;
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
                ppu.vram_buffer = ppu_vram_read(ppu.ppuaddr);
            } else {
                // 直接读取调色板数据，无需缓冲
                data = ppu_vram_read(ppu.ppuaddr);
            }

            // 更新PPU地址
            if (ppu.ppuctrl & 0x04) {
                ppu.ppuaddr += 32; // 垂直增量模式
            } else {
                ppu.ppuaddr += 1;  // 水平增量模式
            }
            break;
        default:
            fprintf(stderr, "incomplete register:[0x%X]!\n", address);
            break;
    }

    return data;
}

void ppu_write(WORD address, uint8_t data)
{
    switch (address) {
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

            ppu_vram_write(ppu.ppuaddr, data);

            if (ppu.ppuctrl & 0x04) {
                ppu.ppuaddr += 32; // 垂直增量模式
            } else {
                ppu.ppuaddr += 1;  // 水平增量模式
            }
            break;
        default:
            fprintf(stderr, "Write to unsupported PPU register: [0x%X]!\n", address);
            // 忽略未定义的写入
            break;
    }
}

