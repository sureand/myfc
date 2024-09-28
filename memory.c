#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include "controller.h"

// TODO: 后期优化, 把整个RAM直接映射64k全部空间

BYTE bus_read(WORD address)
{
    BYTE data = 0;

    /* 由于 0x0800 - 0x1FFF 是CPU RAM 的镜像, 直接取模*/
    if (address <= 0x1FFF) {
        return cpu_read_byte(address & 0x7FF);
    }

    /* ppu 的处理 */
    if (address >= 0x2000 && address <= 0x3FFF) {
        return ppu_read(address & 0x2007);
    }

    /* APU状态寄存器，用于启用或禁用音频通道，并读取APU的状态。*/
    /*apu 的 $4017 是不可读的, 因此在这里过滤*/
    if (is_apu_address(address) && address != 0x4017) {
        return apu_read(address);
    }

    /* 手柄处理 */
    if (address >= 0x4016 && address <= 0x4017) {
        if (address == 0x4016) {

            data = get_pressed_key();

            /* 关闭锁存器, 返回当前的按键状态 */
            if (!is_controller_latch()) {
                data = get_button_state();
            }
            return data | 0x40;
        }

        return data;
    }

    /* Expansion ROM (可选，用于扩展硬件) */
    if (address >= 0x4020 && address <= 0x5FFF) {
        return extend_rom[address - 0x4020];
    }

    /* SRAM: 0x6000-0x7FFF(用于保存记录) */
    if (address >= 0x6000 && address <= 0x7FFF) {
        return sram[address & 0x1FFF];
    }

    /*PRG ROM 主程序*/
    if (address >= 0x8000 && address <= 0xFFFF) {
        return prg_rom_read(address);
    }

    return 0;
}

void bus_write(WORD address, BYTE data)
{
    if (address < 0x1FFF) {
        cpu_write_byte(address & 0x7FF, data);
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        ppu_write(address & 0x2007, data);
        return;
    }

    /* OAM DMA 写入 */
    if (address == 0x4014) {

        WORD dma_address = data << 8;

	    for (int x = 0; x < 256; x++) {

            BYTE value = bus_read(dma_address + x);
            cpu_clock();

            ppu.oam[(ppu.oamaddr + x) & 0xFF] = value;
            cpu_clock();
        }

        /* 奇数周期需要 + 1*/
        if (cpu.cycle & 0x1) {
            cpu_clock();
        }

        return;
    }

    /* APU状态寄存器，用于启用或禁用音频通道，并读取APU的状态。*/
    if (is_apu_address(address)) {
        apu_write(address, data);
        return;
    }

    /* 手柄处理 */
    if (address == 0x4016) {

        uint8_t latch_state = data & 1;
        set_latch_state(latch_state);

        /* 假如关闭锁存器, 那么更新所有按键状态 */
        if (address == 0x4016 && !latch_state) {
            update_controller();
        }

        return;
    }

    /* Expansion ROM (可选，用于扩展硬件) */
    if (address >= 0x4020 && address <= 0x5FFF) {
        extend_rom[address - 0x4020] = data;
    }

    /* SRAM: 0x6000-0x7FFF(用于保存记录) */
    if (address >= 0x6000 && address <= 0x7FFF) {
        sram[address & 0x1FFF] = data;
        return;
    }

    /*PRG ROM 和 CHR ROM 主程序*/
    if (address >= 0x8000 && address <= 0xFFFF) {
        prg_rom_write(address, data);
    }

}
