#include "memory.h"
#include "cpu.h"
#include "ppu.h"

// TODO: 后期优化, 把整个RAM直接映射64k全部空间

BYTE bus_read(WORD address)
{
    /* 由于 0x0800 - 0x1FFF 是CPU RAM 的镜像, 直接取模*/
    if (address <= 0x1FFF) {
        return cpu_read_byte(address & 0x7FF);
    }

    /* ppu 的处理 */
    if (address >= 0x2000 && address <= 0x3FFF) {
        //printf("\nread ppu address: 0X%04X\n", address);
        return ppu_read(address & 0x2007);
    }

    /* APU寄存器，用于控制音频通道（脉冲波、三角波、噪声、DMC） */
    if (address >= 0x4000 && address <= 0x4013) {
        printf("Read from unsupported address: %04X\n", address);
        return 0;
        //TODO: apu_read(address, data); // 需要实现的函数
    }

    /* APU状态寄存器，用于启用或禁用音频通道，并读取APU的状态。*/
    if (address == 0x4015) {
        printf("Read from unsupported address: %04X\n", address);
        return 0;
        //TODO: apu_status_read(address); // 需要实现的函数
    }

    /* 手柄处理 */
    if (address >= 0x4016 && address <= 0x4017) {
        printf("Read from unsupported address: %04X\n", address);
        return 0;
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

    /*PRG ROM 主程序*/
    if (address >= 0x8000 && address <= 0xFFFF) {
        WORD address_range = prg_rom_count > 1 ? 0x7FFF : 0x3FFF;
        return prg_rom[address & address_range];
    }

    printf("\nRead from unsupported address: %04X\n", address);
    _exit(-1);

    return 0;
}

void bus_write(WORD address, BYTE data)
{
    if (address < 0x1FFF) {
        cpu_write_byte(address & 0x7FF, data);
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        printf("write ppu address: 0X%04X, data:[%d]\n", address, data);
        ppu_write(address & 0x2007, data);
        return;
    }

    /* APU 的读写 */
    if (address >= 0x4000 && address <= 0x4013) {
        printf("Write from unsupported address: %04X\n", address);
        //TODO: apu_write(address, data); // 需要实现的函数
        return;
    }

    /* OAM DMA 写入 */
    if (address == 0x4014) {

        WORD dma_address = data << 8;
        if (dma_address >= CPU_RAM_SIZE) {
            printf("Write from unsupported address: %04X\n", dma_address);
            return;
        }

        memcpy(ppu.oamdata, cpu.ram + dma_address, OAM_SIZE);
        return;
    }

    /* APU状态寄存器，用于启用或禁用音频通道，并读取APU的状态。*/
    if (address == 0x4015) {
        printf("Write from unsupported address: %04X\n", address);
        //apu_status_write(data); // 需要实现的函数
        return;
    }

    /* 手柄处理 */
    if (address >= 0x4016 && address <= 0x4017) {
        printf("Write from unsupported address: %04X\n", address);
        //TODO: controller_write(address);
        return;
    }

    /* Expansion ROM (可选，用于扩展硬件) */
    if (address >= 0x4020 && address <= 0x5FFF) {
        extend_rom[address - 0x4020] = data;
    }

    /* SRAM: 0x6000-0x7FFF(用于保存记录) */
    if (address >= 0x6000 && address <= 0x7FFF) {
        sram[address - 0x6000] = data;
        return;
    }

    /*PRG ROM 和 CHR ROM 主程序*/
    if (address >= 0x8000 && address <= 0xFFFF) {

        WORD address_range = prg_rom_count > 1 ? 0x7FFF : 0x3FFF;

        prg_rom[address & address_range] = data;

        return;
    }

}

void mem_init(ROM *rom)
{
    /* 储存ROM页面数目和对应的ROM数据 */
    prg_rom_count = rom->header->prg_rom_count;
    prg_rom = rom->prg_rom;

    chr_rom_count = rom->header->chr_rom_count;
    chr_rom = rom->chr_rom;

    /* 拷贝到 ppu 的vram  */
    memcpy(ppu.vram, chr_rom, CHR_ROM_SIZE);
}
