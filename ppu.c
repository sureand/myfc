
#include "ppu.h"
#include <SDL2/SDL.h>

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


/*调色板数据来自官方资料*/
uint32_t rgb_palette[64] = {
    0xFF757575, 0xFF271B8F, 0xFF0000AB, 0xFF47009F,
    0xFF8F0077, 0xFFAB0013, 0xFFA70000, 0xFF7F0B00,
    0xFF432F00, 0xFF004700, 0xFF005100, 0xFF003F17,
    0xFF1B3F5F, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFFBCBCBC, 0xFF0073EF, 0xFF233BEF, 0xFF8300F3,
    0xFFBF00BF, 0xFFE7005B, 0xFFDB2B00, 0xFFCB4F0F,
    0xFF8B7300, 0xFF009700, 0xFF00AB00, 0xFF00933B,
    0xFF00838B, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFFFFFFFF, 0xFF3FBFFF, 0xFF5F97FF, 0xFFA78BFD,
    0xFFF77BFF, 0xFFFF77B7, 0xFFFF7763, 0xFFFF9B3B,
    0xFFF3BF3F, 0xFF83D313, 0xFF4FDF4B, 0xFF58F898,
    0xFF00EBDB, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFFFFFFFF, 0xFFABE7FF, 0xFFC7D7FF, 0xFFD7CBFF,
    0xFFFFC7FF, 0xFFFFC7DB, 0xFFFFBFB3, 0xFFFFDBAB,
    0xFFFFE7A3, 0xFFE3FFA3, 0xFFABF3BF, 0xFFB3FFCF,
    0xFF9FFFF3, 0xFF000000, 0xFF000000, 0xFF000000
};

void ppu_vram_write(WORD address, BYTE data);

void ppu_init()
{
    memset(&ppu, 0, sizeof(_PPU));

    // 假设 header[6] 的第 0 位决定水平或垂直镜像
    if (mirroring & 0x01) {
        ppu.mirroring = VERTICAL_MIRRORING;
    } else {
        ppu.mirroring = HORIZONTAL_MIRRORING;
    }

    // 检查是否支持四屏镜像
    if (mirroring & 0x08) {
        ppu.mirroring = FOUR_SCREEN_MIRRORING;
    }

    /* 拷贝到 ppu 的vram  */
    memcpy(ppu.vram, chr_rom, CHR_ROM_SIZE);

    // 这里使用rgb 调色板的索引即可.
    for (int i = 0; i < 64; i++) {
        ppu_vram_write(0x3F00 + i, i);
    }
}

// 计算实际的 Name Table 地址
WORD get_name_table_address(WORD address, BYTE mirroring)
{
    address &= 0x0FFF; // 限制在 Name Table 范围内
    switch (mirroring) {
        case HORIZONTAL_MIRRORING:
            return (address < 0x0800) ? (0x2000 + address) : (0x2400 + (address - 0x0800));
        case VERTICAL_MIRRORING:
            return (address < 0x0400 || (address >= 0x0800 && address < 0x0C00)) ? (0x2000 + (address & 0x07FF)) : (0x2400 + (address & 0x03FF));
        case SINGLE_SCREEN_MIRRORING_0:
            return 0x2000 + (address & 0x03FF);
        case SINGLE_SCREEN_MIRRORING_1:
            return 0x2400 + (address & 0x03FF);
        case FOUR_SCREEN_MIRRORING:
            return 0x2000 + address;
        default:
            return 0x2000 + address;
    }
}

// 处理调色板地址的镜像
WORD get_palette_address(WORD address)
{
    address = 0x3F00 | (address & 0x1F); // 限制在 $3F00-$3F1F 范围内
    if (address == 0x3F10 || address == 0x3F14 || address == 0x3F18 || address == 0x3F1C) {
        address -= 0x10; // 镜像 $3F10/$3F14/$3F18/$3F1C 到 $3F00/$3F04/$3F08/$3F0C
    }
    return address;
}

// 读取 VRAM
BYTE ppu_vram_read(WORD address)
{
    address &= 0x3FFF;

    WORD real_address = address;

    if (address < 0x2000) {
        // Pattern tables 区域
        return ppu.vram[address];
    } else if (address < 0x3F00) {
        // Name tables 和 Attribute tables 区域
        if (address >= 0x3000) {
            address -= 0x1000; // 处理 $3000-$3FFF 镜像到 $2000-$2FFF
        }
        real_address = get_name_table_address(address - 0x2000, ppu.mirroring);
        return ppu.vram[real_address];
    } else {
        // Palette 区域
        real_address = get_palette_address(address);
        return ppu.vram[real_address];
    }
}

// 写入 VRAM
void ppu_vram_write(WORD address, BYTE data)
{
    address &= 0x3FFF;

    WORD real_address = address;

    if (address < 0x2000) {
        // Pattern tables 区域
        ppu.vram[address] = data;
    } else if (address < 0x3F00) {
        // Name tables 和 Attribute tables 区域
        if (address >= 0x3000) {
            address -= 0x1000; // 处理 $3000-$3FFF 镜像到 $2000-$2FFF
        }
        real_address = get_name_table_address(address - 0x2000, ppu.mirroring);
        ppu.vram[real_address] = data;

    } else {
        // Palette 区域
        real_address = get_palette_address(address);
        ppu.vram[real_address] = data;
    }
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
            data = ppu.oam[ppu.oamaddr];
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
            //fprintf(stderr, "incomplete register:[0x%X]!\n", address);
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
            ppu.oam[ppu.oamaddr] = data;
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

static inline WORD get_name_table_base()
{
    return 0x2000 + (ppu.ppuctrl & 0x03) * 0x400;
}

/* 根据扫描线来渲染背景 */
void render_background(uint32_t* frame_buffer, int scanline)
{
    //取得以8个像素位一个单位的tile 的纵坐标
    int tile_y = scanline / 8;

    /* 这个是扫描线在单个tile 的行内偏移, 就是相对tile的左上角开始位置的纵向方向的偏移*/
    int row_in_tile = scanline & 0x7;

    for (int tile_x = 0; tile_x < 32; tile_x++) {

        /*根据命名表取得图块的索引, 命名表是按照屏幕的布局排布存储的, 每个字节都是存储的图块的所有*/
        WORD name_table_base = get_name_table_base();
        uint16_t name_table_address = name_table_base + tile_y * 32 + tile_x;
        uint8_t tile_index = ppu_vram_read(name_table_address);

        /* *8 是由于每8个一行，超过8个就是下一行的图块了, 根据 *8 就可以得到一维数组中对应的具体地址 */
        uint16_t attribute_table_address = name_table_base + 0x3C0 + (tile_y / 4) * 8 + (tile_x / 4);
        uint8_t attribute_byte = ppu_vram_read(attribute_table_address);

        /* 根据图块索引取得调试板索引的位置*/
        uint8_t shift = ((tile_y & 2) << 1) + (tile_x & 2);

        /* shift 指的是调色板索引在属性表字节中的位置, 调色板索引通过两个位来表示, shift 表示的范围只有1、3、4、6 */
        /* 通过 &0x3 提取这两个位出来 */
        uint8_t palette_index = (attribute_byte >> shift) & 0x03;

        /* 取得所在行的像素点对应的高低平面字节*/
        /* 根据图案表取得颜色值, 每个位都是一个颜色, 对应的是8个列， pattern_table_address 16字节中的每一个字节对应一行 */
        uint16_t pattern_table_address = (tile_index * 16) + ((ppu.ppuctrl & 0x10) ? 0x1000 : 0);
        uint8_t tile_lsb = ppu_vram_read(pattern_table_address + row_in_tile);
        uint8_t tile_msb = ppu_vram_read(pattern_table_address + row_in_tile + 8);

        /*从左到右取得每个像素值, 一个字节对应8列*/
        for (int col = 0; col < 8; col++) {

            uint8_t color_index = 0x00;

            /*pixel_value 是由两个位组成的 2 位值，用于索引调色板, 因此tile_msb 需要 << 1 以便和后面的 tile_lsb 做 bit or 运算*/
            uint8_t pixel_value = ((tile_msb >> (7 - col)) & 1) << 1 | ((tile_lsb >> (7 - col)) & 1);

            /* 取得颜色值, 更新到 frame_buffer 中 */
            WORD addr = palette_index * 4 + pixel_value;
            color_index = ppu_vram_read(0X3F00 + addr);
            frame_buffer[scanline * 256 + (tile_x * 8 + col)] = rgb_palette[color_index];
        }
    }
}

/* 根据扫描线来渲染精灵 */
void render_sprites(uint32_t* frame_buffer, int scanline)
{
    BYTE sprite_0_hit_detected = 0;
    BYTE sprites_on_scanline = 0; // 计数在当前扫描线上渲染的精灵数量

    /* 遍历64 个精灵 */
    for (int i = 0; i < 64; i++) {
        uint8_t y_position = ppu.oam[i * 4];
        uint8_t tile_index = ppu.oam[i * 4 + 1];
        uint8_t attributes = ppu.oam[i * 4 + 2];
        uint8_t x_position = ppu.oam[i * 4 + 3];

        /* 跳过非扫描线的精灵 */
        int sprite_height = (ppu.ppuctrl & 0x20) ? 16 : 8;
        if (scanline < y_position || scanline >= (y_position + sprite_height))
            continue;

        // 增加当前扫描线上的精灵数量
        sprites_on_scanline++;
        if (sprites_on_scanline > 8) {
            // 设置 sprite 溢出 标志
            ppu.ppustatus |= 0x20; // 设置 PPU 状态寄存器的第 5 位
            continue;
        }

        /* 计算当前图块与扫描线相对高度, 即在vram 的相对位置, 用来确定渲染的具体的像素 */
        int y_in_tile = scanline - y_position;

        /* 处理垂直翻转 */
        BYTE flip_vertical = attributes & 0x80;

        /* 获取tile 的行偏移 */
        int v_y = flip_vertical ? (sprite_height - 1 - y_in_tile) : y_in_tile;

        /* 计算图案表地址 */
        uint16_t pattern_table_address = ((ppu.ppuctrl & 0x08) ? 0x1000 : 0) + (tile_index * 16);

        /* 假如是 8 * 16 的, 需要从新算地址 */
        if (sprite_height == 16) {
            /* 8x16 由两个 8 * 8 tile 组成, 每一帧都是相同大小的精灵, 每次从偶数开始算就是对的。 */
            tile_index &= 0xFE;
            pattern_table_address = ((ppu.ppuctrl & 0x08) ? 0x1000 : 0) + (tile_index * 16);
            if (v_y >= 8) {
                /* 已经渲染到了8 * 16 的下半部分, 那么地址需要 +16 */
                pattern_table_address += 16;

                /* 确保从下一个tile 开始算偏移 */
                v_y -= 8;
            }
        }

        uint8_t tile_lsb = ppu_vram_read(pattern_table_address + v_y);
        uint8_t tile_msb = ppu_vram_read(pattern_table_address + v_y + 8);

        /* 取得具体得子调色板的索引, 4 - 7 */
        uint8_t palette_index = (attributes & 0x03) + 4;

        BYTE flip_horizontal = attributes & 0x40;
        BYTE sprite_behind_background = attributes & 0x20;

        /* 处理水平翻转, 分别翻转高低字节的每一个bit */
        for (int x = 0; x < 8; x++) {

            uint8_t color_index = 0x00;

            int h_x = flip_horizontal ? (7 - x) : x;

            /* 翻转低字节的一个bit */
            uint8_t lsb_bit = (tile_lsb >> (7 - h_x)) & 1;

            /* 翻转高字节的一个bit */
            uint8_t msb_bit = (tile_msb >> (7 - h_x)) & 1;

            /* 每个图块的像素由两个位（bit）组成，这两个位分别来自低位平面和高位平面， 从而取得颜色索引完整字节 */
            uint8_t pixel_value = (msb_bit << 1) | lsb_bit;
            if (pixel_value != 0) {
                /* palette_index * 4 + pixel_value 取得具体颜色索引, 用来取得每一个像素 */
                WORD addr = palette_index * 4 + pixel_value;
                color_index = ppu_vram_read(0X3F00 + addr);
            }

            /* 确保像素不超出屏幕边界 */
            int screen_x = x_position + x;
            if (screen_x < 256 && scanline < 240) {
                /* 检查精灵优先级和背景像素 */
                uint32_t background_color = frame_buffer[scanline * 256 + screen_x];
                if (color_index != 0 && (!sprite_behind_background || background_color == 0)) {
                    frame_buffer[scanline * 256 + screen_x] = rgb_palette[color_index];
                }

                // 检查精灵 0 命中, 背景颜色和精灵颜色都不是透明的情况即精灵0命中
                if (i == 0 && color_index != 0 && background_color != 0) {
                    sprite_0_hit_detected = 1;
                }
            }
        }
    }

    // 设置精灵 0 命中标志
    if (sprite_0_hit_detected) {
        ppu.ppustatus |= 0x40; // 设置 PPU 状态寄存器的第 6 位
    }
}

void step_ppu(SDL_Renderer* renderer, SDL_Texture* texture)
{
    static uint32_t frame_buffer[256 * 240] = {0x00};

    // 1. 处理当前扫描线
    if (ppu.scanline < 240) {

        // 可见扫描线，渲染背景和精灵
        if (ppu.cycle == 0) {

            // 清屏并渲染背景和精灵
            render_background(frame_buffer, ppu.scanline);

            render_sprites(frame_buffer, ppu.scanline);

            SDL_UpdateTexture(texture, NULL, frame_buffer, 256 * sizeof(uint32_t));
        }
    } else if (ppu.scanline == 241 && ppu.cycle == 1) {

        // VBlank开始
        ppu.ppustatus |= 0x80;  // 设置VBlank标志

        // 生成NMI中断
        if (ppu.ppuctrl & 0x80) {
            cpu_interrupt_NMI();
        }
    } else if (ppu.scanline >= 261) {
        // VBlank结束，重置VBlank标志
        ppu.ppustatus &= ~0x80;
        ppu.scanline = -1;  // 重置扫描线计数器
    }

    // 2. 更新周期和扫描线计数器
    ppu.cycle++;
    if (ppu.cycle >= 341) {
        ppu.cycle = 0;
        ppu.scanline++;
    }
}