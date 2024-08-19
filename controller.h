#include "common.h"

#define TOTAL_KEYS (8)

uint8_t get_button_state();
void set_button_state(int button, BYTE pressed);
BYTE get_pressed_key();
void handle_key(SDL_Keycode key, BYTE pressed);
void update_controller();
BYTE is_controller_latch();
void set_latch_state(BYTE state);
