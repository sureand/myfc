#include "common.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
// 假设已有的CPU和PPU的结构及函数声明
extern void stepCPU();
extern void stepPPU();
extern void cpu_interrupt_NMI();
extern uint8_t cpu_read(uint16_t address);

typedef struct {
    uint8_t ppuctrl; // PPU控制寄存器
    // 其他PPU相关状态和寄存器
} PPU;

// 全局PPU实例
PPU ppu;

static void run()
{
    int i;
    for (i = 0; i < 3; i++) {
        stepCPU(); // 执行一个CPU周期
    }

    stepPPU(); // 执行一个PPU周期
}

// NMI处理
static void handleNMI()
{
    // PPU在VBlank期间向CPU发送NMI, bit7
    if (ppu.ppuctrl & 0x80) {
        cpu_interrupt_NMI();
    }

    // 清除NMI标志
    ppu.ppuctrl &= ~0x80;
}

bool some_exit_condition() {
    // 示例退出条件，可以根据具体需求实现
    return false;
}

void render_background_optimized(uint32_t* frame_buffer);
void render_sprites_optimized(uint32_t* frame_buffer);

void cpu_step() {
    // 模拟CPU执行一个周期
    printf("CPU step executed\n");
}

void render_background_optimized(uint32_t* frame_buffer)
{
    int x, y, tile_x, tile_y;
    uint16_t name_table_address = 0x2000;
    uint16_t attribute_table_address = name_table_address + 0x3C0;

    for (tile_y = 0; tile_y < 30; tile_y++) {
        for (tile_x = 0; tile_x < 32; tile_x++) {

            uint16_t tile_index = ppu.vram[name_table_address + tile_y * 32 + tile_x];
            uint16_t pattern_table_address = (tile_index * 16) + ((ppu.registers.ppuctrl & 0x10) ? 0x1000 : 0);

            for (y = 0; y < 8; y++) {
                uint8_t tile_lsb = ppu.vram[pattern_table_address + y];
                uint8_t tile_msb = ppu.vram[pattern_table_address + y + 8];

                for (x = 0; x < 8; x++) {
                    uint8_t pixel_value = ((tile_msb >> (7 - x)) & 1) << 1 | ((tile_lsb >> (7 - x)) & 1);

                    uint16_t attribute_address = attribute_table_address + ((tile_y / 4) * 8) + (tile_x / 4);
                    uint8_t attribute_byte = ppu.vram[attribute_address];
                    uint8_t palette_index = 0;

                    if ((tile_x % 4) < 2) {
                        if ((tile_y % 4) < 2) {
                            palette_index = attribute_byte & 0x03;
                        } else {
                            palette_index = (attribute_byte >> 4) & 0x03;
                        }
                    } else {
                        if ((tile_y % 4) < 2) {
                            palette_index = (attribute_byte >> 2) & 0x03;
                        } else {
                            palette_index = (attribute_byte >> 6) & 0x03;
                        }
                    }

                    uint8_t color_index = ppu.palette[palette_index * 4 + pixel_value];
                    frame_buffer[(tile_y * 8 + y) * 256 + (tile_x * 8 + x)] = color_index;
                }
            }
        }
    }
}

void render_sprites_optimized(uint32_t* frame_buffer)
{
    int i, x, y;

    for (i = 0; i < 64; i++) {
        uint8_t y_position = ppu.oam[i * 4];
        uint8_t tile_index = ppu.oam[i * 4 + 1];
        uint8_t attributes = ppu.oam[i * 4 + 2];
        uint8_t x_position = ppu.oam[i * 4 + 3];

        uint16_t pattern_table_address = (tile_index * 16) + ((ppu.registers.ppuctrl & 0x08) ? 0x1000 : 0);
        bool flip_horizontal = attributes & 0x40;
        bool flip_vertical = attributes & 0x80;
        uint8_t palette_index = (attributes & 0x03) + 4;

        for (y = 0; y < 8; y++) {
            uint8_t tile_lsb = ppu.vram[pattern_table_address + (flip_vertical ? (7 - y) : y)];
            uint8_t tile_msb = ppu.vram[pattern_table_address + (flip_vertical ? (7 - y) : y) + 8];

            for (x = 0; x < 8; x++) {
                uint8_t pixel_value = ((tile_msb >> (flip_horizontal ? x : (7 - x))) & 1) << 1 |
                                      ((tile_lsb >> (flip_horizontal ? x : (7 - x))) & 1);

                if (pixel_value != 0) {
                    uint8_t color_index = ppu.palette[palette_index * 4 + pixel_value];
                    frame_buffer[(y_position + y) * 256 + (x_position + x)] = color_index;
                }
            }
        }
    }
}

void stepPPU(SDL_Renderer* renderer, SDL_Texture* texture)
{
    uint32_t frame_buffer[256 * 240];

    // 1. 处理当前扫描线
    if (ppu.scanline < 240) {
        // 可见扫描线，渲染背景和精灵
        if (ppu.cycle == 0) {
            // 清屏并渲染背景和精灵
            render_background_optimized(frame_buffer);
            render_sprites_optimized(frame_buffer);

            // 更新纹理
            SDL_UpdateTexture(texture, NULL, frame_buffer, 256 * sizeof(uint32_t));

            // 清除渲染器
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
            SDL_RenderClear(renderer);

            // 复制纹理到渲染器
            SDL_RenderCopy(renderer, texture, NULL, NULL);

            // 显示渲染内容
            SDL_RenderPresent(renderer);
        }
    } else if (ppu.scanline == 241) {
        // VBlank开始
        ppu.registers.ppustatus |= 0x80;  // 设置VBlank标志
        // 生成NMI中断
        if (ppu.registers.ppuctrl & 0x80) {
            cpu_interrupt_NMI();
        }
    } else if (ppu.scanline >= 261) {
        // VBlank结束，重置VBlank标志
        ppu.registers.ppustatus &= ~0x80;
        ppu.scanline = -1;  // 重置扫描线计数器
        ppu.frame_complete = true;
    }

    // 2. 更新周期和扫描线计数器
    ppu.cycle++;
    if (ppu.cycle >= 341) {
        ppu.cycle = 0;
        ppu.scanline++;
    }
}

void handle_user_event(SDL_Renderer* renderer, SDL_Texture* texture)
{
    const uint64_t CPU_CYCLES_PER_FRAME = 29830; // 大约为1/60秒内的CPU周期数

    for (uint64_t i = 0; i < CPU_CYCLES_PER_FRAME; ++i) {
        cpu_step();
        stepPPU(renderer, texture);
        stepPPU(renderer, texture);
        stepPPU(renderer, texture);
    }
}

void main_loop(SDL_Window* window, SDL_Renderer* renderer)
{
    uint8_t running = 1;
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);

    // 创建一个定时器，每秒触发60次
    SDL_TimerID timerID = SDL_AddTimer(1000 / 60, timer_callback, NULL);

    if (timerID == 0) {
        fprintf(stderr, "SDL_AddTimer failed! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    while (running) {
        SDL_Event event;
        while (SDL_WaitEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
                break;
            } else if (event.type == SDL_USEREVENT) {
                handle_user_event(renderer, texture);

                // 检查退出条件
                if (some_exit_condition()) {
                    running = 0;
                    break;
                }
            }
        }
    }

    // 清理SDL
    SDL_DestroyTexture(texture);
    SDL_RemoveTimer(timerID);
}

int main()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("NES Emulator",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          256, 240, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 初始化PPU
    ppu_init(&ppu);

    // 启动模拟器主循环
    main_loop(window, renderer);

    // 清理资源
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}