#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include "controller.h"

static BYTE *read_page_ptr[0x100];
static BYTE *write_page_ptr[0x100];
static BYTE page_cache_ready = 0;

static inline BYTE read_controller_port(WORD address)
{
    BYTE data = 0;

    if (address == 0x4016) {
        data = get_pressed_key();
        if (!is_controller_latch()) {
            data = get_button_state();
        }
        return data | 0x40;
    }

    return data;
}

static inline BYTE slow_bus_read(WORD address)
{
    if (address >= 0x2000 && address <= 0x3FFF) {
        return ppu_read(address & 0x2007);
    }

    if (address >= 0x4000 && address <= 0x401F) {
        if (is_apu_address(address) && address != 0x4017) {
            return apu_read(address);
        }

        if (address >= 0x4016 && address <= 0x4017) {
            return read_controller_port(address);
        }

        return 0;
    }

    if (address >= 0x4020 && address <= 0x40FF) {
        return extend_rom[address - 0x4020];
    }

    if (address >= 0x8000) {
        return prg_rom_read(address);
    }

    return 0;
}

static inline void slow_bus_write(WORD address, BYTE data)
{
    if (address >= 0x2000 && address <= 0x3FFF) {
        ppu_write(address & 0x2007, data);
        return;
    }

    if (address == 0x4014) {
        WORD dma_address = data << 8;

        for (int x = 0; x < 256; x++) {
            BYTE value = bus_read(dma_address + x);
            cpu_clock();

            ppu.oam[(ppu.oamaddr + x) & 0xFF] = value;
            cpu_clock();
        }

        ppu_invalidate_sprite_cache();

        if (cpu.cycle & 0x1) {
            cpu_clock();
        }

        return;
    }

    if (is_apu_address(address)) {
        apu_write(address, data);
        return;
    }

    if (address == 0x4016) {
        uint8_t latch_state = data & 1;
        set_latch_state(latch_state);

        if (!latch_state) {
            update_controller();
        }

        return;
    }

    if (address >= 0x4020 && address <= 0x40FF) {
        extend_rom[address - 0x4020] = data;
        return;
    }

    if (address >= 0x8000) {
        prg_rom_write(address, data);
    }
}

void bus_init_page_cache()
{
    memset(read_page_ptr, 0, sizeof(read_page_ptr));
    memset(write_page_ptr, 0, sizeof(write_page_ptr));

    for (int page = 0x00; page <= 0x1F; ++page) {
        BYTE *ram_page = cpu.ram + (((page << 8) & 0x7FF));
        read_page_ptr[page] = ram_page;
        write_page_ptr[page] = ram_page;
    }

    for (int page = 0x41; page <= 0x5F; ++page) {
        BYTE *expansion_page = extend_rom + (((page - 0x40) << 8));
        read_page_ptr[page] = expansion_page;
        write_page_ptr[page] = expansion_page;
    }

    for (int page = 0x60; page <= 0x7F; ++page) {
        BYTE *sram_page = sram + (((page - 0x60) << 8));
        read_page_ptr[page] = sram_page;
        write_page_ptr[page] = sram_page;
    }

    page_cache_ready = 1;
}

static inline void ensure_page_cache()
{
    if (!page_cache_ready) {
        bus_init_page_cache();
    }
}

BYTE bus_read(WORD address)
{
    ensure_page_cache();

    BYTE *page = read_page_ptr[address >> 8];
    if (page) {
        return page[address & 0xFF];
    }

    return slow_bus_read(address);
}

void bus_write(WORD address, BYTE data)
{
    ensure_page_cache();

    BYTE *page = write_page_ptr[address >> 8];
    if (page) {
        page[address & 0xFF] = data;
        return;
    }

    slow_bus_write(address, data);
}
