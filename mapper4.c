#include "mapper4.h"

static MMC4_REGISTER mmc4_reg;

static inline size_t get_prg_address(WORD address)
{
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
            return mmc4_reg.second_last_bank + (address & 0x1FFF);
        }
    } else {

        //0x8000 - 0x9FFF
        if (address < 0xA000) {
            return mmc4_reg.second_last_bank + (address & 0x1FFF);
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
    return mmc4_reg.last_bank + (address & 0x1FFF);
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
            ppu.mirroring = (data & 0x1) ? HORIZONTAL_MIRRORING : VERTICAL_MIRRORING;
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

        //bank 0
        if (address >= 0x000 && address <= 0x3FF) {
            return (mmc4_reg.R[0] & 0xFE) * 0x400 + (address & 0x3FF);
        }

        /*  bank 1
         * 由于nesdev 上面说明 bank 的计数仍然是使用的1k， https://www.nesdev.org/wiki/MMC3
         * 因此奇数的bank 通过bank1 需要通过bank0 * 0x400 + 0x400 的方式来计算.
         * R0 and R1 ignore the bottom bit, as the value written still counts banks in 1KB units but odd numbered banks can't be selected.
         */
        if (address >= 0x400 && address <= 0x7FF) {
            return  ((mmc4_reg.R[0] * 0x400) + 0x400) + (address & 0x3FF);
        }

        if (address >= 0x800 && address <= 0xBFF) {
            return (mmc4_reg.R[1] & 0xFE) * 0x400 + (address & 0x3FF);
        }

        if (address >= 0xC00 && address <= 0xFFF) {
            return ((mmc4_reg.R[1] * 0x400) + 0x400) + (address & 0x3FF);
        }

        if (address >= 0x1000 && address <= 0x13FF) {
            return (mmc4_reg.R[2]) * 0x400 + (address & 0x3FF);
        }

        if (address >= 0x1400 && address <= 0x17FF) {
            return (mmc4_reg.R[3]) * 0x400 + (address & 0x3FF);
        }

        if (address >= 0x1800 && address <= 0x1BFF) {
            return (mmc4_reg.R[4]) * 0x400 + (address & 0x3FF);
        }

        //0x1C00 - 0x1FFF
        return (mmc4_reg.R[5]) * 0x400 + (address & 0x3FF);
    }

    if (address >= 0x000 && address <= 0x3FF) {
        return (mmc4_reg.R[2] * 0x400) + (address & 0x3FF);
    }

    if (address >= 0x400 && address <= 0x7FF) {
        return (mmc4_reg.R[3] * 0x400) + (address & 0x3FF);
    }

    if (address >= 0x800 && address <= 0xBFF) {
        return (mmc4_reg.R[4]) * 0x400 + (address & 0x3FF);
    }

    if (address >= 0xC00 && address <= 0xFFF) {
        return (mmc4_reg.R[5]) * 0x400 + (address & 0x3FF);
    }

    if (address >= 0x1000 && address <= 0x13FF) {
        return (mmc4_reg.R[0] & 0xFE) * 0x400 + (address & 0x3FF);
    }

    if (address >= 0x1400 && address <= 0x17FF) {
        return (mmc4_reg.R[0] * 0x400 + 0x400) + (address & 0x3FF);
    }

    if (address >= 0x1800 && address <= 0x1BFF) {
        return (mmc4_reg.R[1] & 0xFE) * 0x400 + (address & 0x3FF);
    }

    return (mmc4_reg.R[1] * 0x400 + 0x400) + (address & 0x3FF);
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
    BYTE need_irq = 0;

    if (mmc4_reg.irq_counter == 0 || mmc4_reg.irq_reload) {
        mmc4_reg.irq_counter = mmc4_reg.irq_latch;
        mmc4_reg.irq_reload = 0;
    } else {
        mmc4_reg.irq_counter--;
        need_irq = mmc4_reg.irq_counter == 0;
    }

    if (need_irq && mmc4_reg.irq_enable) {
        set_irq();
    }
}

void mapper_reset4()
{
    memset(&mmc4_reg, 0, sizeof(MMC4_REGISTER));

    mmc4_reg.last_bank = get_current_rom()->header->prg_rom_count * PRG_ROM_PAGE_SIZE - 0x2000;
    mmc4_reg.second_last_bank = get_current_rom()->header->prg_rom_count * PRG_ROM_PAGE_SIZE - 0x4000;
}