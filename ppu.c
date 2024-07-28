
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

#define IS_VISIBLE(x, y) ((x) < 256 && (y) < 240)
#define IS_TRANSPARENT(color) ((color) == 0)
#define PALETTE_ADDR(palette, pixel) ((palette) * 4 + (pixel))

/*调色板数据来自官方资料*/
uint32_t rgb_palette[64] = {
    0x00757575, 0x00271B8F, 0x000000AB, 0x0047009F,
    0x008F0077, 0x00AB0013, 0x00A70000, 0x007F0B00,
    0x00432F00, 0x00004700, 0x00005100, 0x00003F17,
    0x001B3F5F, 0x00000000, 0x00000000, 0x00000000,

    0x00BCBCBC, 0x000073EF, 0x00233BEF, 0x008300F3,
    0x00BF00BF, 0x00E7005B, 0x00DB2B00, 0x00CB4F0F,
    0x008B7300, 0x00009700, 0x0000AB00, 0x0000933B,
    0x0000838B, 0x00000000, 0x00000000, 0x00000000,

    0x00FFFFFF, 0x003FBFFF, 0x005F97FF, 0x00A78BFD,
    0x00F77BFF, 0x00FF77B7, 0x00FF7763, 0x00FF9B3B,
    0x00F3BF3F, 0x0083D313, 0x004FDF4B, 0x0058F898,
    0x0000EBDB, 0x00000000, 0x00000000, 0x00000000,

    0x00FFFFFF, 0x00ABE7FF, 0x00C7D7FF, 0x00D7CBFF,
    0x00FFC7FF, 0x00FFC7DB, 0x00FFBFB3, 0x00FFDBAB,
    0x00FFE7A3, 0x00E3FFA3, 0x00ABF3BF, 0x00B3FFCF,
    0x009FFFF3, 0x00000000, 0x00000000, 0x00000000
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
        return chr_rom[address];
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
        // Pattern tables 区域 (通常不允许写入，CHR ROM 是只读的)
        // 通常情况下，这里会抛弃写操作或抛出错误
        printf("Attempted write to CHR ROM address 0x%04X, which is read-only.\n", address);
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

            // 读取 PPUSTATUS 时重置 w 寄存器
            ppu.w = 0;

            // 清除 VBlank 标志 (bit 7)
            ppu.ppustatus &= 0x7F;
            break;
        case 0x2004:
            data = ppu.oam[ppu.oamaddr];
            break;
        case 0x2007:
            if (ppu.v < 0x3F00) {
                data = ppu.vram_buffer;
                ppu.vram_buffer = ppu_vram_read(ppu.v);
            } else {
                // 直接读取调色板数据，无需缓冲
                data = ppu_vram_read(ppu.v);
            }

            ppu.v += (ppu.ppuctrl & 0x04) ? 32 : 1; // 垂直/水平增量模式
            break;

        default:
            fprintf(stderr, "Read from unsupported PPU register: [0x%X]!\n", address);
            break;
    }

    return data;
}

void ppu_write(WORD address, uint8_t data)
{
    switch (address) {
        case 0x2000: // PPUCTRL
            ppu.ppuctrl = data;
            ppu.t = (ppu.t & 0xF3FF) | ((data & 0x03) << 10); // 更新临时 VRAM 地址
            break;
        case 0x2001: // PPUMASK
            ppu.ppumask = data;
            break;
        case 0x2003: // OAMADDR
            ppu.oamaddr = data;
            break;
        case 0x2004: // OAMDATA
            ppu.oam[ppu.oamaddr] = data;
            ppu.oamaddr = (ppu.oamaddr + 1) & 0xFF; // 循环 OAM 地址
            break;
        case 0x2005: // PPUSCROLL
            if (ppu.w == 0) {
                ppu.t = (ppu.t & 0xFFE0) | (data >> 3);
                ppu.x = data & 0x07;
                ppu.w = 1;
            } else {
                ppu.t = (ppu.t & 0x8FFF) | ((data & 0x07) << 12);
                ppu.t = (ppu.t & 0xFC1F) | ((data & 0xF8) << 2);
                ppu.w = 0;
            }
            break;
        case 0x2006: // PPUADDR
            if (ppu.w == 0) {
                ppu.t = (ppu.t & 0x80FF) | ((data & 0x3F) << 8);
                ppu.w = 1;
            } else {
                ppu.t = (ppu.t & 0xFF00) | data;
                ppu.v = ppu.t;
                ppu.w = 0;
            }
            break;
        case 0x2007: // PPUDATA
            ppu_vram_write(ppu.v, data);
            ppu.v += (ppu.ppuctrl & 0x04) ? 32 : 1; // 垂直/水平增量模式
            break;
        default:
            fprintf(stderr, "Write to unsupported PPU register: [0x%X]!\n", address);
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
            if (pixel_value != 0x00) {
                WORD addr = palette_index * 4 + pixel_value;
                color_index = ppu_vram_read(0X3F00 + addr);
            } else {
                /* 透明的情况使用背景色 */
                color_index = ppu_vram_read(0X3F00);
            }

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
        int sprite_behind_background = attributes & 0x20;

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

            if (!IS_TRANSPARENT(pixel_value)) {
                WORD addr = PALETTE_ADDR(palette_index, pixel_value);
                color_index = ppu_vram_read(0x3F00 + addr);
            }

            int screen_x = x_position + x;

            if (IS_VISIBLE(screen_x, scanline)) {
                uint32_t background_color = frame_buffer[scanline * 256 + screen_x];

                /* 非透明而且(不需要显示在背景前面或者背景是透明的) 那么显示出来 */
                if (!IS_TRANSPARENT(pixel_value) && (!sprite_behind_background || IS_TRANSPARENT(background_color))) {
                    frame_buffer[scanline * 256 + screen_x] = rgb_palette[color_index];
                }

                if (i == 0 && !IS_TRANSPARENT(pixel_value) && !IS_TRANSPARENT(background_color)) {
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