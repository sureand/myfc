#include "controller.h"

static uint8_t controller_state = 0x00;
static uint8_t shift_register = 0x00;
static uint8_t controller_latch = 0;

static const SDL_Scancode key_bindings[TOTAL_KEYS] = {
    SDL_SCANCODE_J, // A
    SDL_SCANCODE_K, // B
    SDL_SCANCODE_U, // Select
    SDL_SCANCODE_RETURN, // Start
    SDL_SCANCODE_W, //Up
    SDL_SCANCODE_S, //Down
    SDL_SCANCODE_A, //Left
    SDL_SCANCODE_D, //Rigth
};

BYTE is_controller_latch()
{
    return controller_latch;
}

void set_latch_state(BYTE state)
{
    controller_latch = state;
}

uint8_t get_button_state()
{
    uint8_t state = shift_register & 1;
    shift_register >>= 1;

    return state;
}

void set_button_state(int button, BYTE pressed)
{
    if (pressed) {
        controller_state |= (1 << button);
        return;
    }

    controller_state &= ~(1 << button);

    return;
}

void handle_key(SDL_Keycode key, BYTE pressed)
{
    switch (key) {
        case SDLK_j: set_button_state(0, pressed); break; // A button
        case SDLK_k: set_button_state(1, pressed); break; // B button
        case SDLK_u: set_button_state(2, pressed); break; // Select button
        case SDLK_RETURN: set_button_state(3, pressed); break; // Start button
        case SDLK_w: set_button_state(4, pressed); break; // Up button
        case SDLK_s: set_button_state(5, pressed); break; // Down button
        case SDLK_a: set_button_state(6, pressed); break; // Left button
        case SDLK_d: set_button_state(7, pressed); break; // Right button
    }
}

BYTE get_pressed_key()
{
    const uint8_t* state = SDL_GetKeyboardState(NULL);
    for (uint8_t button = 0; button < TOTAL_KEYS; ++button) {
        if (state[key_bindings[button]]) {
            return button;
        }
    }

    return 0;
}

void update_controller()
{
    shift_register = controller_state;
}