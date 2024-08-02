#include "load_rom.h"
#include "cpu.h"
#include "ppu.h"

#define NTSC_CPU_CYCLES_PER_FRAME 29781 // 精确值，以避免窗口卡顿
#define PAL_CPU_CYCLES_PER_FRAME 33248 // 精确值，以避免窗口卡顿

#define PPU_CYCLES_PER_CPU_CYCLE 3

#define FRAME_DURATION 1000 / 60 // 60 FPS

uint32_t timer_callback(uint32_t interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);

    return interval;
}

void process_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            exit(0);
        } else if (event.type == SDL_KEYDOWN) {
            handle_key(event.key.keysym.sym, 1);
        } else if (event.type == SDL_KEYUP) {
            handle_key(event.key.keysym.sym, 0);
        }
    }
}

void wait_for_frame()
{
    static uint32_t last_frame_time = 0;
    uint32_t current_time = SDL_GetTicks();

    // 计算当前帧和上一帧之间的时间差
    uint32_t frame_time = current_time - last_frame_time;

    // 如果当前帧时间小于每帧的持续时间，则等待
    if (frame_time < FRAME_DURATION) {
        uint32_t wait_time = FRAME_DURATION - frame_time;
        uint32_t start_wait_time = SDL_GetTicks();

        while (SDL_GetTicks() - start_wait_time < wait_time) {

            // 在等待期间处理事件
            process_events();

            // 为了防止 CPU 占用过高，稍微延迟一下
            SDL_Delay(1);
        }
    }

    // 更新上一帧的时间
    last_frame_time = SDL_GetTicks();
}

void handle_user_event(SDL_Renderer* renderer, SDL_Texture* texture)
{
    const uint64_t CPU_CYCLES_PER_FRAME = NTSC_CPU_CYCLES_PER_FRAME;
    uint64_t total_cpu_cycles = 0; // 用于记录总的CPU周期数

    uint64_t frame_start_cycles = total_cpu_cycles; // 记录每帧开始时的CPU周期数

    while (total_cpu_cycles < CPU_CYCLES_PER_FRAME) {

        // 处理事件
        process_events();

        // 执行 CPU 步骤，并获取执行该指令所需的实际周期数
        int actual_cpu_cycles = step_cpu();

        // 根据实际 CPU 周期数执行相应的 PPU 步骤
        for (int i = 0; i < actual_cpu_cycles; i++) {

            // 执行 PPU 步骤
            for (int j = 0; j < 3; j++) {
                step_ppu(renderer, texture);
            }
        }

        // 更新 CPU 周期计数
       total_cpu_cycles += actual_cpu_cycles;
    }
}

void main_loop(SDL_Renderer *renderer)
{
    uint8_t running = 1;
    int width = 256, height = 240;
    float scale_x = 1.0f, scale_y = 1.0f;

    // 创建一个定时器，每秒触发60次
    SDL_TimerID timerID = SDL_AddTimer(500 / 60, timer_callback, NULL);

    if (timerID == 0) {
        fprintf(stderr, "SDL_AddTimer failed! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    // 创建一个纹理，用于渲染PPU输出
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
                break;
            } else if (event.type == SDL_USEREVENT) {
                handle_user_event(renderer, texture);
            }
            else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    // 当窗口大小改变时，更新窗口的宽度和高度
                    width = event.window.data1;
                    height = event.window.data2;

                    // 重新设置渲染器的缩放比例
                    scale_x = (float)width / 256.0f;
                    scale_x = (float)height / 240.0f;
                    SDL_RenderSetScale(renderer, scale_x, scale_y);
                }
            }

            // 设置绘图颜色为黑色
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

            // 清空屏幕（填充为黑色）
            SDL_RenderClear(renderer);

            // 将纹理复制到渲染器
            SDL_RenderCopy(renderer, texture, NULL, NULL);

            // 显示渲染结果
            SDL_RenderPresent(renderer);
        }

    }

    // 清理SDL
    SDL_DestroyTexture(texture);
    SDL_RemoveTimer(timerID);
}

int start()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow("MY FC",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          512, 480, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_RenderSetLogicalSize(renderer, 256, 240);

    // 启动模拟器主循环
    main_loop(renderer);

    // 清理资源
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

void release_memory(ROM *rom)
{
    FREE(rom->header);
    FREE(rom);

    FREE(chr_rom);
    FREE(prg_rom);
}

ROM *fc_init()
{
    ROM *rom = load_rom("test.nes");

    mem_init(rom);

    ppu_init();
    cpu_init();

    return rom;
}

void fc_release(ROM *rom)
{
    FREE(rom->header);
    FREE(rom);

    FREE(chr_rom);
    FREE(prg_rom);
}

#undef main
int main(int argc, char *argv[])
{
    ROM *rom = fc_init();

    start();

    fc_release(rom);

    return 0;
}