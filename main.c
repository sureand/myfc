#include "load_rom.h"
#include "cpu.h"
#include "ppu.h"

#define NTSC_CPU_CYCLES_PER_FRAME 33200 // 精确值，以避免窗口卡顿
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

void wait_for_frame()
{
    static uint32_t last_frame_time = 0;
    uint32_t current_time = SDL_GetTicks();

    // 计算当前帧和上一帧之间的时间差
    uint32_t frame_time = current_time - last_frame_time;

    // 如果当前帧时间小于每帧的持续时间，则等待
    if (frame_time < FRAME_DURATION) {
        SDL_Delay(FRAME_DURATION - frame_time);
    }

    // 更新上一帧的时间
    last_frame_time = SDL_GetTicks();
}

void handle_user_event(SDL_Renderer* renderer, SDL_Texture* texture)
{
    uint64_t cpu_cycles = 0;
    const uint64_t CPU_CYCLES_PER_FRAME = NTSC_CPU_CYCLES_PER_FRAME;
    SDL_Event event;

    // 初始化定时器
    uint32_t frame_start = SDL_GetTicks();

    while (cpu_cycles < CPU_CYCLES_PER_FRAME) {

        // 处理事件
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                exit(0);
            }
            //读取手柄等外部输入 update_controller_state(&event);
        }

        // 执行 CPU 和 PPU 步骤
        step_cpu();
        for (int j = 0; j < PPU_CYCLES_PER_CPU_CYCLE; j++) {
            step_ppu(renderer, texture);
        }

        cpu_cycles++;
    }

    // 检查并处理 NMI
    if (ppu.scanline == 240) {
        cpu_interrupt_NMI();
    }

    // 限制帧率
    wait_for_frame();
}

void main_loop(SDL_Renderer *renderer)
{
    uint8_t running = 1;
    int windowWidth = 256, windowHeight = 240;
    float scaleX = 1.0f, scaleY = 1.0f;

    // 创建一个定时器，每秒触发60次
    SDL_TimerID timerID = SDL_AddTimer(1000 / 60, timer_callback, NULL);

    if (timerID == 0) {
        fprintf(stderr, "SDL_AddTimer failed! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    // 创建一个纹理，用于渲染PPU输出
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);

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
                    windowWidth = event.window.data1;
                    windowHeight = event.window.data2;

                    // 重新设置渲染器的缩放比例
                    scaleX = (float)windowWidth / 256.0f; // 假设原始宽度为800
                    scaleY = (float)windowHeight / 240.0f; // 假设原始高度为600
                    SDL_RenderSetScale(renderer, scaleX, scaleY);
                }
            }
            SDL_RenderSetScale(renderer, scaleX, scaleY);
            // 渲染模拟器的画面
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
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
                                          256, 240, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
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
    ROM * rom = load_rom("nestest.nes");

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
    ROM * rom = fc_init();

    handle_reset();

    start();

    fc_release(rom);

    return 0;
}