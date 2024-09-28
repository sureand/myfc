#include "load_rom.h"
#include "cpu.h"
#include "ppu.h"
#include "mapper.h"

#define NTSC_CPU_CYCLES_PER_FRAME 29781 // 精确值，以避免窗口卡顿
#define PAL_CPU_CYCLES_PER_FRAME 33248 // 精确值，以避免窗口卡顿

#define PPU_CYCLES_PER_CPU_CYCLE 3

#define FRAME_DURATION 1000 / 60 // 60 FPS

void reload_rom(const char *filename)
{
    fc_release();

    set_current_rom(load_rom(filename));

    cpu_reset();
    ppu_reset();
    mapper_reset();
}

char *get_file_name(char *filename, const char *filepath)
{
    assert(filepath);

    #ifdef __WIN32__
        const char *delim = strrchr(filepath, '\\');
    #else
        const char *delim = strrchr(filepath, '/');
    #endif

    return strcpy(filename, (delim ? delim + 1 : filepath));
}

static inline SDL_bool is_load_rom()
{
    return rom_loaded;
}

static inline void set_load_rom(SDL_bool state)
{
    rom_loaded = state;
}

void reset_rom(const char *filepath)
{
    get_file_name(window_title, filepath);

    // 清除渲染器
    set_load_rom(SDL_FALSE);

    if (!current_rom) {
        fc_init(filepath);
        set_load_rom(SDL_TRUE);
        return;
    }

    reload_rom(filepath);

    set_load_rom(SDL_TRUE);
}

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

void reset_windows_size(SDL_Renderer* renderer, int width, int height)
{
    float scale_x = (float)width / SCREEN_WIDTH;
    float scale_y = (float)height / SCREEN_HEIGHT;

    SDL_RenderSetScale(renderer, scale_x, scale_y);
}

int process_events(SDL_Renderer* renderer)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                exit(0);
                break;
            case SDL_KEYDOWN:
                handle_key(event.key.keysym.sym, 1);
                break;
            case SDL_KEYUP:
                handle_key(event.key.keysym.sym, 0);
                break;
            case SDL_DROPFILE:
                reset_rom(event.drop.file);
                return 1;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    int width = event.window.data1;
                    int height = event.window.data2;
                    reset_windows_size(renderer, width, height);
                    break;
                }
                break;
            default:
                break;
        }
    }

    return 0;
}

int main_loop(void *arg)
{
    EMULATOR_SCREEN *screen = (EMULATOR_SCREEN *)arg;

    // 创建一个纹理，用于渲染PPU输出
    SDL_Texture *texture = SDL_CreateTexture(screen->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    set_SDLdevice(screen->renderer, texture);

    uint32_t total_cpu_cycles = 1;

    for (;;) {

        if (!is_load_rom()) {
            SDL_Delay(1);
            continue;
        }

        total_cpu_cycles += step_cpu();
    }

    // 清理SDL
    SDL_DestroyTexture(texture);

    return 0;
}

int start()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(window_title,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2,
                                          SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    DEBUG_PRINT("Renderer name: %s\n", info.name);
    if (info.flags & SDL_RENDERER_PRESENTVSYNC) {
        DEBUG_PRINT("VSync is enabled\n");
    } else {
        DEBUG_PRINT("VSync is not enabled\n");
    }

    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    if (setup_sdl_audio() == -1) {
        return -1;
    }

    // 模拟器的窗口
    EMULATOR_SCREEN screen;
    screen.window = window;
    screen.renderer = renderer;

    SDL_Thread *thread = SDL_CreateThread(main_loop, "main_loop", &screen);
    if (!thread) {
        printf("emulator thread creation failed!\n");
        return -1;
    }

    for (;;) {
        process_events(renderer);
        SDL_Delay(1);
    }

    int status;
    SDL_WaitThread(thread, &status);

    // 清理资源
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    cleanup_sdl_audio();
    SDL_Quit();

    return 0;
}

void fc_release()
{
    FREE(get_current_rom()->header);
    FREE(get_current_rom()->body);
    FREE(current_rom);
}

void fc_init(const char *filename)
{
    set_current_rom(load_rom(filename));

    //FIXME: 由于链接顺序的问题, 只能放在前面, what the fuck!
    mapper_init();

    apu_init();
    cpu_init();
    ppu_init();
}

void set_init_state()
{
    set_current_rom(NULL);

    set_load_rom(SDL_FALSE);
    strcpy(window_title,  "NES Emulator");
}

#undef main
int main(int argc, char *argv[])
{
    set_init_state();

    start();

    fc_release();

    return 0;
}