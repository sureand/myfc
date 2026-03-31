#include "controller.h"

static SDL_SpinLock controller_lock = 0;
static uint8_t controller_state = 0x00;
static uint8_t shift_register = 0x00;
static uint8_t controller_latch = 0;

static const SDL_Scancode scancode_bindings[TOTAL_KEYS] = {
    SDL_SCANCODE_J,      // A
    SDL_SCANCODE_K,      // B
    SDL_SCANCODE_U,      // Select
    SDL_SCANCODE_RETURN, // Start
    SDL_SCANCODE_W,      // Up
    SDL_SCANCODE_S,      // Down
    SDL_SCANCODE_A,      // Left
    SDL_SCANCODE_D,      // Right
};

static const SDL_Keycode keycode_bindings[TOTAL_KEYS] = {
    SDLK_j,
    SDLK_k,
    SDLK_u,
    SDLK_RETURN,
    SDLK_w,
    SDLK_s,
    SDLK_a,
    SDLK_d,
};

static inline void lock_controller()
{
    SDL_AtomicLock(&controller_lock);
}

static inline void unlock_controller()
{
    SDL_AtomicUnlock(&controller_lock);
}

static int resolve_button(SDL_Keycode key, SDL_Scancode scancode)
{
    for (int button = 0; button < TOTAL_KEYS; ++button) {
        if (key == keycode_bindings[button] || scancode == scancode_bindings[button]) {
            return button;
        }
    }

    return -1;
}

BYTE is_controller_latch()
{
    BYTE state;

    lock_controller();
    state = controller_latch;
    unlock_controller();

    return state;
}

void set_latch_state(BYTE state)
{
    lock_controller();
    controller_latch = !!state;
    if (controller_latch) {
        shift_register = controller_state;
    }
    unlock_controller();
}

uint8_t get_button_state()
{
    uint8_t state;

    lock_controller();
    if (controller_latch) {
        state = controller_state & 1;
    } else {
        state = shift_register & 1;
        shift_register >>= 1;
    }
    unlock_controller();

    return state;
}

void set_button_state(int button, BYTE pressed)
{
    lock_controller();
    if (pressed) {
        controller_state |= (1u << button);
    } else {
        controller_state &= ~(1u << button);
    }
    unlock_controller();
}

void handle_key(SDL_Keycode key, SDL_Scancode scancode, BYTE pressed)
{
    int button = resolve_button(key, scancode);
    if (button >= 0) {
        set_button_state(button, pressed);
    }
}

BYTE get_pressed_key()
{
    BYTE state;

    lock_controller();
    state = controller_state & 1;
    unlock_controller();

    return state;
}

void update_controller()
{
    lock_controller();
    shift_register = controller_state;
    unlock_controller();
}

void clear_controller_state()
{
    lock_controller();
    controller_state = 0;
    shift_register = 0;
    controller_latch = 0;
    unlock_controller();
}
