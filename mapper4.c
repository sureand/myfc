#include "mapper4.h"

static MMC4_REGISTER mmc4_reg;

static inline size_t get_prg_address(WORD address)
{
    size_t last_bank = (get_current_rom()->header->prg_rom_count - 1) * PRG_ROM_PAGE_SIZE - 0x4000;
    size_t last_second_bank = (get_current_rom()->header->prg_rom_count - 2) * PRG_ROM_PAGE_SIZE - 0x2000;

    //prg_mode 默认为0, 处理了开机时候默认从最后的两个bank 加载
    if (mmc4_reg.prg_mode == 0) {

        //0x8000 - 0x9FFF
        if (address < 0xA000) {
            return (mmc4_reg.R[6] & 0x3F) * 0x2000 + (address & 0x1FFF);
        }

        //0xA000 - 0xBFFF
        if (address < 0xC000) {
            return (mmc4_reg.R[7] & 0x3F) * 0x2000 + (address & 0x1FFF);
        }

        //0xC000 - 0xDFFF
        if (address < 0xE000) {
            return last_second_bank + (address & 0x1FFF);
        }
    } else {

        //0x8000 - 0x9FFF
        if (address < 0xA000) {
            return last_second_bank + (address & 0x1FFF);
        }

        //0xA000 - 0xBFFF
        if (address < 0xC000) {
            return (mmc4_reg.R[7] & 0x3F) * 0x2000 + (address & 0x1FFF);
        }

        //0xC000 - 0xDFFF
        if (address < 0xE000) {
            return (mmc4_reg.R[6] & 0x3F) * 0x2000 + (address & 0x1FFF);
        }
    }

    // 0xE000 - 0xFFFF 都是映射到最后一个bank
    return last_bank + (address & 0x1FFF);
}

BYTE prg_rom_read4(WORD address)
{
    size_t _address = get_prg_address(address);
    return get_current_rom()->prg_rom[_address];
}

void prg_rom_write4(WORD address, BYTE data)
{

    if (address >= 0x8000 && address<= 0x9FFF) {

        //偶数是bank select
        if (!(address & 0x1)) {
            //指定下次更新数据的目标寄存器
            mmc4_reg.target_register = data & 0x7;
            mmc4_reg.prg_mode = data & 0x40;
            mmc4_reg.chr_inversion = data & 0x80;
            return;
        }

        //使用上次的寄存器来存储数据
        mmc4_reg.R[mmc4_reg.target_register] = data;
    }


    //$A000-$BFFE
    if (address >= 0xA000 && address<= 0xBFFF) {
         if (!(address & 0x1)) {
            ppu.mirroring = (data & 0x1) ? VERTICAL_MIRRORING : HORIZONTAL_MIRRORING;
            return;
         }
        //下面这两个是暂时用不上的
        use_prg_ram = !!(data & 0x80);
        mmc4_reg.write_protect = !!(data & 0x40);
    }

    //$C000-$DFFF
    if (address >= 0xC000 && address<= 0xDFFF) {
         if (!(address & 0x1)) {
            mmc4_reg.irq_latch = data;
            return;
         }
        mmc4_reg.irq_counter = 0;
        mmc4_reg.irq_reload = 1;
    }

    //$E000-$FFFF
    if (address >= 0xE000 && address<= 0xFFFF) {
        mmc4_reg.irq_enable = address & 0x1;
    }
}

static inline size_t get_chr_address(WORD address)
{
    if (mmc4_reg.chr_inversion == 0) {

        if (address < 0x800) {
            return (mmc4_reg.R[0] & 0xFE) * 0x800 + (address & 0x7FF);
        }

        if (address < 0x1000) {
            return (mmc4_reg.R[1] & 0xFE) * 0x800 + (address & 0x7FF);
        }

        if (address < 0x1400) {
            return (mmc4_reg.R[2]) * 0x400 + (address & 0x3FF);
        }

        if (address < 0x1800) {
            return (mmc4_reg.R[3]) * 0x400 + (address & 0x3FF);
        }

        if (address < 0x1C00) {
            return (mmc4_reg.R[4]) * 0x400 + (address & 0x3FF);
        }

        return (mmc4_reg.R[5]) * 0x400 + (address & 0x3FF);
    }

    if (address < 0x400) {
        return (mmc4_reg.R[2] * 0x400) + (address & 0x3FF);
    }

    if (address < 0x800) {
        return (mmc4_reg.R[3] * 0x400) + (address & 0x3FF);
    }

    if (address < 0xC00) {
        return (mmc4_reg.R[4]) * 0x400 + (address & 0x3FF);
    }

    if (address < 0x1000) {
        return (mmc4_reg.R[5]) * 0x400 + (address & 0x3FF);
    }

    if (address < 0x1800) {
        return (mmc4_reg.R[0] & 0xFE) * 0x800 + (address & 0x7FF);
    }

    return (mmc4_reg.R[1] & 0xFE) * 0x800 + (address & 0x7FF);
}

BYTE chr_rom_read4(WORD address)
{
    if (get_current_rom()->header->chr_rom_count == 0) {
        return ppu.vram[address];
    }

    size_t _address = get_chr_address(address);
    return get_current_rom()->chr_rom[_address];
}

void chr_rom_write4(WORD address, BYTE data)
{
    if (get_current_rom()->header->chr_rom_count == 0) {
        ppu.vram[address] = data;
    }
}

void irq_scanline4()
{
    BYTE need_irq = (mmc4_reg.irq_counter == 0);

    if (mmc4_reg.irq_counter == 0 || mmc4_reg.irq_reload) {
        mmc4_reg.irq_counter = mmc4_reg.irq_latch;
        mmc4_reg.irq_reload = 0;
    } else {
        mmc4_reg.irq_counter--;
    }

    if (need_irq && mmc4_reg.irq_enable) {
        set_irq();
    }
}

void mapper_reset4()
{
    memset(&mmc4_reg, 0, sizeof(MMC4_REGISTER));
}