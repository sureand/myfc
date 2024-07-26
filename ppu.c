
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


typedef struct {
    BYTE r;
    BYTE g;
    BYTE b;
} color;

/*调色板数据来自官方资料*/
color nes_palette[64] = {
    {0x75, 0x75, 0x75}, {0x27, 0x1B, 0x8F}, {0x00, 0x00, 0xAB}, {0x47, 0x00, 0x9F},
    {0x8F, 0x00, 0x77}, {0xAB, 0x00, 0x13}, {0xA7, 0x00, 0x00}, {0x7F, 0x0B, 0x00},
    {0x43, 0x2F, 0x00}, {0x00, 0x47, 0x00}, {0x00, 0x51, 0x00}, {0x00, 0x3F, 0x17},
    {0x1B, 0x3F, 0x5F}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xBC, 0xBC, 0xBC}, {0x00, 0x73, 0xEF}, {0x23, 0x3B, 0xEF}, {0x83, 0x00, 0xF3},
    {0xBF, 0x00, 0xBF}, {0xE7, 0x00, 0x5B}, {0xDB, 0x2B, 0x00}, {0xCB, 0x4F, 0x0F},
    {0x8B, 0x73, 0x00}, {0x00, 0x97, 0x00}, {0x00, 0xAB, 0x00}, {0x00, 0x93, 0x3B},
    {0x00, 0x83, 0x8B}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF}, {0x3F, 0xBF, 0xFF}, {0x5F, 0x97, 0xFF}, {0xA7, 0x8B, 0xFD},
    {0xF7, 0x7B, 0xFF}, {0xFF, 0x77, 0xB7}, {0xFF, 0x77, 0x63}, {0xFF, 0x9B, 0x3B},
    {0xF3, 0xBF, 0x3F}, {0x83, 0xD3, 0x13}, {0x4F, 0xDF, 0x4B}, {0x58, 0xF8, 0x98},
    {0x00, 0xEB, 0xDB}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF}, {0xAB, 0xE7, 0xFF}, {0xC7, 0xD7, 0xFF}, {0xD7, 0xCB, 0xFF},
    {0xFF, 0xC7, 0xFF}, {0xFF, 0xC7, 0xDB}, {0xFF, 0xBF, 0xB3}, {0xFF, 0xDB, 0xAB},
    {0xFF, 0xE7, 0xA3}, {0xE3, 0xFF, 0xA3}, {0xAB, 0xF3, 0xBF}, {0xB3, 0xFF, 0xCF},
    {0x9F, 0xFF, 0xF3}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};

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

    /* 初始化调色板 */
    ppu.palette = &ppu.vram[0x3F00];

    // 这里使用rgb 调色板的索引即可.
    for (int i = 0; i < 64; i++) {
        ppu.palette[i] = i;
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

/* 根据扫描线来渲染背景*/
void render_background(uint8_t* frame_buffer, int scanline)
{
    int tile_y = scanline / 8;
    int row_in_tile = scanline % 8;

    for (int tile_x = 0; tile_x < 32; tile_x++) {

        /*根据命名表取得图块的索引, 命名表是按照屏幕的布局排布存储的, 每个字节都是存储的图块的所有*/
        WORD name_table_base = get_name_table_base();
        uint16_t name_table_address = name_table_base + tile_y * 32 + tile_x;
        uint8_t tile_index = ppu_vram_read(name_table_address);

        /* 属性表的存储也是按照屏幕布局来存储的, 根据属性表取得调色板得索引, 把宽256 * 240 分成 32 * 32 的 小图块, 那么可以有8 行 7.5列 */
        /* 再把32 * 32 划分, 就可以得到 4个 16 * 16 的图块, 那么tile_y / 4 就是取得这个16 * 16 的图块对应的图块*/
        /* *8 是由于每8个一行，超过8个就是下一行的图块了, 根据 *8 就可以得到一维数组中对应的具体地址 */
        uint16_t attribute_table_address = name_table_base + 0x3C0 + (tile_y / 4) * 8 + (tile_x / 4) + 0x400;
        uint8_t attribute_byte = ppu_vram_read(attribute_table_address);

        /* 根据图块索引取得调试板索引的位置*/
        uint8_t shift = ((tile_y & 2) << 1) + (tile_x & 2);

        /* shift 指的是调色板索引在属性表字节中的位置, 调色板索引通过两个位来表示, shift 表示的范围只有1、3、4、6 */
        /* 通过 &0x3 提取这两个位出来 */
        uint8_t palette_index = (attribute_byte >> shift) & 0x03;

        /* 取得 高低平面字节*/
        /* 根据图案表取得属性value, 使用 scanline/4 会更加直观 */
        uint16_t pattern_table_address = (tile_index * 16) + ((ppu.ppuctrl & 0x10) ? 0x1000 : 0);
        uint8_t tile_lsb = ppu_vram_read(pattern_table_address + row_in_tile);
        uint8_t tile_msb = ppu_vram_read(pattern_table_address + row_in_tile + 8);

        /*从左到右取得每个像素值*/
        for (int col = 0; col < 8; col++) {

            /*pixel_value 是由两个位组成的 2 位值，用于索引调色板, 因此tile_msb 需要 << 1 以便和后面的 tile_lsb 做 bit or 运算*/
            uint8_t pixel_value = ((tile_msb >> (7 - col)) & 1) << 1 | ((tile_lsb >> (7 - col)) & 1);
            if (pixel_value != 0) {

                /* 取得颜色值, 更新到 frame_buffer 中 */
                WORD addr = palette_index * 4 + pixel_value;
                uint8_t color_index = ppu.palette[addr];
                frame_buffer[scanline * 256 + (tile_x * 8 + col)] = color_index;
            }
        }
    }
}

/* 根据扫描线来渲染精灵*/
void render_sprites(uint8_t* frame_buffer, int scanline)
{
    /* 遍历64 个精灵 */
    for (int i = 0; i < 64; i++) {
        uint8_t y_position = ppu.oam[i * 4];
        uint8_t tile_index = ppu.oam[i * 4 + 1];
        uint8_t attributes = ppu.oam[i * 4 + 2];
        uint8_t x_position = ppu.oam[i * 4 + 3];

        /*  跳过非扫描线的精灵 */
        if (scanline < y_position || scanline >= (y_position + (ppu.ppuctrl & 0x20 ? 16 : 8)))
            continue;

        /* 8*16 精灵 的地址是 0x1000, 每个tile 16 字节, 通过index 取得在vram 中的位置 */
        uint16_t pattern_table_address = (tile_index * 16) + ((ppu.ppuctrl & 0x08) ? 0x1000 : 0);
        BYTE flip_horizontal = attributes & 0x40;
        BYTE flip_vertical = attributes & 0x80;

        /* 取得具体得子调色板的索引, 4 - 7 */
        uint8_t palette_index = (attributes & 0x03) + 4;

        /* 计算当前图块与扫描线相对高度, 即在vram 的相对位置, 用来确定渲染的具体的像素 */
        int y_in_tile = scanline - y_position;
        if (ppu.ppuctrl & 0x20) {
            if (y_in_tile >= 8) {

                /* 取得下半部分的偏移, 8 * 16 的图块，其实就是使用两个 8 * 8 的图块来存储的 */
                y_in_tile -= 8;
                pattern_table_address += 16;
            }
        }

        /* 处理垂直翻转, 翻转图块的高低字节 */
        /* 由于是nes 想要节省内存和增加灵活性, 因此把像素使用颜色索引的方式压缩在vram 中, 导致这么恶心的反人类的实现 */
        int v_y = flip_vertical ? (7 - y_in_tile) : y_in_tile;
        uint8_t tile_lsb = ppu_vram_read(pattern_table_address + v_y);
        uint8_t tile_msb = ppu_vram_read(pattern_table_address + v_y + 8);

        /* 处理水平翻转, 分别翻转高低字节的每一个bit */
        for (int x = 0; x < 8; x++) {

            int h_x = flip_horizontal ? x : (7 - x);

            /* 翻转低字节的一个bit */
            uint8_t lsb_bit = (tile_lsb >> h_x) & 1;

            /* 翻转高字节的一个bit */
            uint8_t msb_bit = (tile_msb >> h_x) & 1;

            /* 每个图块的像素由两个位（bit）组成，这两个位分别来自低位平面和高位平面， 从而取得颜色索引完整字节 */
            uint8_t pixel_value = (msb_bit << 1) | lsb_bit;
            if (pixel_value != 0) {

                /* palette_index * 4 + pixel_value 取得具体颜色索引, 用来取得每一个像素*/
                WORD addr = palette_index * 4 + pixel_value;
                uint8_t color_index = ppu.palette[addr & 0x1F];
                frame_buffer[scanline * 256 + (x_position + x)] = color_index;
            }
        }
    }

}

void convert_palette(uint8_t* frame_buffer, uint32_t* rgb_frame_buffer)
{
    for (int i = 0; i < 256 * 240; ++i) {
        uint8_t index = frame_buffer[i];
        uint32_t color_index = ppu.palette[index];
        color c = nes_palette[color_index]; // 获取调色板中的颜色
        // 使用 ARGB 格式，将 Alpha 通道放在最高字节，接下来是红色、绿色和蓝色通道
        uint32_t color_value = (0xFF << 24) | (c.r << 16) | (c.g << 8) | c.b;
        rgb_frame_buffer[i] = color_value;
    }
}

void step_ppu(SDL_Renderer* renderer, SDL_Texture* texture)
{
    static uint8_t frame_buffer[256 * 240] = {0x00};
    static uint32_t rgb_frame_buffer[256 * 240] = {0x00}; // 用于存储 RGB 颜色值

    // 1. 处理当前扫描线
    if (ppu.scanline < 240) {

        // 可见扫描线，渲染背景和精灵
        if (ppu.cycle == 0) {

            // 清屏并渲染背景和精灵
            render_background(frame_buffer, ppu.scanline);

            render_sprites(frame_buffer, ppu.scanline);

            // 将 frame_buffer 中的索引值转换为实际的 RGB 颜色
            convert_palette(frame_buffer, rgb_frame_buffer);

            SDL_UpdateTexture(texture, NULL, rgb_frame_buffer, 256 * sizeof(uint32_t));
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