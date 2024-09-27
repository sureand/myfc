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

void wait_for_frame(SDL_Renderer* renderer)
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
            if (process_events(renderer)) {
                return;
            }

            // 为了防止 CPU 占用过高，稍微延迟一下
            SDL_Delay(1);
        }
    }

    // 更新上一帧的时间
    last_frame_time = SDL_GetTicks();
}

void fce_execute(SDL_Renderer* renderer, SDL_Texture* texture, int* frame_count)
{
    int total_cpu_cycles = 0;

    while (total_cpu_cycles < NTSC_CPU_CYCLES_PER_FRAME) {

        // 处理事件
        if (process_events(renderer)) {
            return;
        }
        total_cpu_cycles += step_cpu();;
    }

    // 增加帧计数
    (*frame_count)++;

    wait_for_frame(renderer);
}

void main_loop(SDL_Window *window, SDL_Renderer *renderer)
{
    uint8_t running = 1;

    // 创建一个纹理，用于渲染PPU输出
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    set_SDLdevice(renderer, texture);

    Uint32 start_time = SDL_GetTicks();
    int frame_count = 0;
    char title[256];

    while (running) {

        process_events(renderer);

        if (is_load_rom()) {
            // 每秒更新一次 FPS
            Uint32 current_time = SDL_GetTicks();
            if (current_time - start_time >= 1000) {
                float fps = frame_count / ((current_time - start_time) / 1000.0f);
                snprintf(title, sizeof(title), "%s - FPS: %.2f", window_title, fps);
                SDL_SetWindowTitle(window, title);
                start_time = current_time;
                frame_count = 0;
            }
            fce_execute(renderer, texture, &frame_count);
        }
    }

    // 清理SDL
    SDL_DestroyTexture(texture);
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

    // 启动模拟器主循环
    main_loop(window, renderer);

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
    apu_init();
    set_init_state();

    start();

    fc_release();

    return 0;
}