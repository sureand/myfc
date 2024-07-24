#include "load_rom.h"
#include "cpu.h"
#include "ppu.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_events.h"

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

void handle_user_event(SDL_Renderer* renderer, SDL_Texture* texture)
{
    uint64_t i;
    const uint64_t CPU_CYCLES_PER_FRAME = 29830; // 大约为1/60秒内的CPU周期数    // // 每执行一个CPU周期，执行三个PPU周期
    for (i = 0; i < CPU_CYCLES_PER_FRAME; ++i) {

        step_cpu();

        for (int j = 0; j < 3; i++) {
            step_ppu(renderer, texture);
        }
        cpu_interrupt_NMI();
    }
}

void main_loop(SDL_Renderer *renderer)
{
    uint8_t running = 1;

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
        while (SDL_WaitEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
                break;
            } else if (event.type == SDL_USEREVENT) {
                handle_user_event(renderer, texture);

                // 检查退出条件, break

                // 渲染模拟器的画面
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);
            }
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

    SDL_Window *window = SDL_CreateWindow("NES Emulator",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          256, 240, SDL_WINDOW_SHOWN);
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

    ppu_init();

    mem_init(rom);

    init_cpu();

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

    start();

    fc_release(rom);

    return 0;
}