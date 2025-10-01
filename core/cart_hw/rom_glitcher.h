// rom_glitcher.h

#ifndef _ROM_GLITCHER_H_
#define _ROM_GLITCHER_H_

#include "rom_glitcher_defines.h"
#include "rom_glitcher_types.h"
#include "shared.h"
#include <stdbool.h>
#include <stdint.h>

extern int32_t rg_menu_button;
extern bool rg_swap_buttons;
extern bool rg_menu_visible;
//extern char rg_log[1024];

extern const struct retro_game_info* rg_last_game;
extern bool libretro_supports_bitmasks;
extern char g_rom_dir[256];
extern char g_rom_name[256];
extern retro_log_printf_t log_cb;
extern retro_video_refresh_t video_cb;
extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;
extern retro_environment_t environ_cb;

void rg_init(uint8_t* rom_data, uint32_t size);
void rg_deinit(void);
void rg_force_stop(void);
void rg_game_reset(void);
void rg_game_save_state(void);
void rg_game_load_state(void);
void rg_launch_glitcher(void);
void rg_handle_input(const t_bitmap* bitmap, const int* vwidth, const int* vheight);

#endif