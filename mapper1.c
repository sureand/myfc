#include "mapper1.h"

static mmc1_regISTER mmc1_reg;

static inline void reset_register()
{
    mmc1_reg.write_count = 0;
    mmc1_reg.shift_reg = 0;
    mmc1_reg.prg_mode = 0x3;
}

void update_mirroring(BYTE mirroring)
{
    switch (mirroring) {
        case 0x0: ppu.mirroring = SINGLE_SCREEN_MIRRORING_0; break;
        case 0x1: ppu.mirroring = SINGLE_SCREEN_MIRRORING_1; break;
        case 0x2: ppu.mirroring = VERTICAL_MIRRORING; break;
        default:  ppu.mirroring = HORIZONTAL_MIRRORING; break;
    }
}

void update_bank_setting(WORD address, BYTE value)
{
    switch (address & 0xE000) {
        case 0x8000:
            update_mirroring(value & 0x3);
            mmc1_reg.prg_mode = (value & 0xC) >> 2;
            mmc1_reg.chr_mode = value & 0x10;
            break;
        //chr bank0
        case 0xA000:
            mmc1_reg.chr_bank_number1 = value & 0x1F;
            break;
        //chr bank1
        case 0xC000:
            mmc1_reg.chr_bank_number2 = value & 0x1F;
            break;
        case 0xE000:
            // prg bank
            mmc1_reg.prg_bank_number = value & 0xF;
            if (value & 0x10) {
                use_prg_ram = 1;
            }
            break;
    }
}

size_t get_prg_address(WORD address)
{
    size_t _address = 0xFFFF;

    switch (mmc1_reg.prg_mode & 0x3) {
        case 0x2:
            //0x8000 - 0xC000 映射到第一个bank
            _address = address & 0x3FFF;
            if (address >= 0xC000) {
                //每次切换 16KB
                _address = (mmc1_reg.prg_bank_number * 0x4000) + (address & 0x3FFF);
            }
            break;
        case 0x3:
            _address = (mmc1_reg.prg_bank_number * 0x4000) + (address & 0x3FFF);
            if (address >= 0xC000) {
                // 固定到最后一个bank
                size_t last_bank = (get_current_rom()->header->prg_rom_count - 1) * 0x4000;
                _address = last_bank + (address & 0x3FFF);
            }
            break;
        default:
            //映射32k, 忽略低位, 需要右移1
             _address = (mmc1_reg.prg_bank_number >> 1) * 0x8000 + (address & 0x7FFF);
            break;
    }

    return _address;
}

BYTE prg_rom_read1(WORD address)
{
    size_t _address = get_prg_address(address);
    return get_current_rom()->prg_rom[_address];
}

void prg_rom_write1(WORD address, BYTE data)
{
    /*写入任意bit7 是1 的值, 都会清空shift register*/
    if (data & 0x80) {
        reset_register();
        return;
    }

    mmc1_reg.shift_reg >>= 1;
    mmc1_reg.shift_reg |= (data & 0x1) << 4;
    mmc1_reg.write_count++;
    if (mmc1_reg.write_count < 5) {
        return;
    }

    update_bank_setting(address, mmc1_reg.shift_reg);
    reset_register();
}

size_t get_chr_address(WORD address)
{
    // 处理 0x0000 - 0xFFF
    if (address < 0x1000) {
         if (mmc1_reg.chr_mode & 0x1) {
            return (mmc1_reg.chr_bank_number1 >> 1) * 0x2000 + (address & 0x1FFF);
         }
        return mmc1_reg.chr_bank_number1 * 0x1000 + (address & 0xFFF);
    }

    //处理0x1000 - 0x1FFF 的情况
    return mmc1_reg.chr_bank_number2 * 0x1000 + (address & 0xFFF);
}

BYTE chr_rom_read1(WORD address)
{
    if (get_current_rom()->header->chr_rom_count == 0) {
        return ppu.vram[address];
    }

    size_t _address = get_chr_address(address);
    return get_current_rom()->chr_rom[_address];
}

void chr_rom_write1(WORD address, BYTE data)
{
    if (get_current_rom()->header->chr_rom_count == 0) {
        ppu.vram[address] = data;
    }
}

void mapper_reset1()
{
    memset(&mmc1_reg, 0, sizeof(mmc1_regISTER));
    mmc1_reg.prg_mode = 0x3;
    mmc1_reg.chr_mode = 0x10;
}
