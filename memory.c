#include "memory.h"
#include "cpu.h"
#include "ppu.h"

// TODO: 后期优化, 把整个RAM直接映射64k全部空间

//按键状态寄存器, 手柄这块还没想好放哪里, 先统一放这里
uint8_t controller_state = 0x00;

//移位寄存器
uint8_t shift_register = 0x00;


void set_button_state(int button, BYTE pressed)
{
    if (pressed) {
        controller_state |= (1 << button);
        return;
    }

    controller_state &= ~(1 << button);

    return;
}

void handle_key(SDL_Keycode key, BYTE pressed)
{
    switch (key) {
        case SDLK_j: set_button_state(0, pressed); break; // A button
        case SDLK_k: set_button_state(1, pressed); break; // B button
        case SDLK_u: set_button_state(2, pressed); break; // Select button
        case SDLK_RETURN: set_button_state(3, pressed); break; // Start button
        case SDLK_w: set_button_state(4, pressed); break; // Up button
        case SDLK_s: set_button_state(5, pressed); break; // Down button
        case SDLK_a: set_button_state(6, pressed); break; // Left button
        case SDLK_d: set_button_state(7, pressed); break; // Right button
    }
}

uint8_t get_button_state()
{
    uint8_t state = shift_register & 1;
    shift_register >>= 1;

    return state;
}

BYTE bus_read(WORD address)
{
    /* 由于 0x0800 - 0x1FFF 是CPU RAM 的镜像, 直接取模*/
    if (address <= 0x1FFF) {
        return cpu_read_byte(address & 0x7FF);
    }

    /* ppu 的处理 */
    if (address >= 0x2000 && address <= 0x3FFF) {
        BYTE data = ppu_read(address & 0x2007);
        return data;
    }

    /* APU寄存器，用于控制音频通道（脉冲波、三角波、噪声、DMC） */
    if (address >= 0x4000 && address <= 0x4013) {
        return 0;
        //TODO: apu_read(address, data); // 需要实现的函数
    }

    /* APU状态寄存器，用于启用或禁用音频通道，并读取APU的状态。*/
    if (address == 0x4015) {
        //printf("Read from unsupported address: %04X\n", address);
        return 0;
        //TODO: apu_status_read(address); // 需要实现的函数
    }

    /* 手柄处理 */
    if (address >= 0x4016 && address <= 0x4017) {
        return get_button_state();
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

    /* APU 的读写 */
    if (address >= 0x4000 && address <= 0x4013) {
        //printf("Write from unsupported address: %04X\n", address);
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

        memcpy(ppu.oam, cpu.ram + dma_address, OAM_SIZE);
        return;
    }

    /* APU状态寄存器，用于启用或禁用音频通道，并读取APU的状态。*/
    if (address == 0x4015) {
        //printf("Write from unsupported address: %04X\n", address);
        //apu_status_write(data); // 需要实现的函数
        return;
    }

    /* 手柄处理 */
    if (address >= 0x4016 && address <= 0x4017) {

        //重置手柄状态
        if (address == 0x4016 && (data & 1)) {
            shift_register = controller_state;
        }

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
        printf("Attempted write to PRG ROM address 0x%X - 0x%X\n", address, data);
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

    mirroring = rom->header->flag1;
}
