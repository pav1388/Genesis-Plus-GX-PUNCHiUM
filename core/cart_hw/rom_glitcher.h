// rom_glitcher.h
// perfect_genius - glitcher idea, pav13 - implementation

#ifndef _ROM_GLITCHER_H_
#define _ROM_GLITCHER_H_

#define RG_DISABLED_KEY -2 // random number that is not gamepad button

#include "shared.h"
#include <stdint.h>
#include <stdbool.h>

extern const struct retro_game_info* rg_last_game; // info to reload current game
extern int32_t rg_menu_button;
extern bool rg_swap_buttons;
extern bool libretro_supports_bitmasks;
extern char g_rom_dir[256];
extern char g_rom_name[256];
extern retro_log_printf_t log_cb;
extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;
extern retro_environment_t environ_cb;

#ifdef COMPRESSED_OPCODE_TABLE
extern void init_m68k_opcode_valid(void);
extern bool m68k_opcode_valid(uint16_t opcode);
#else
extern const bool m68k_opcode_valid_table[0x10000];
#endif // COMPRESSED_OPCODE_TABLE

static const char* get_label_main(void);
static const char* get_label_list_of_found(void);
static const char* get_label_branch_allowed(void);
static const char* get_label_menu_list_of_found(void);
static const char* get_label_menu_branch_allowed(void);
static void menu_item_modified_selected_glitch(void);
static void menu_item_branch_allowed(void);
static void menu_item_prev_page(void);
static void menu_item_next_page(void);
static void menu_item_open_options(void);
static void menu_item_pause_effect(void);
static void menu_item_open_list_of_found(void);
static void menu_item_open_branch_allowed(void);
static void menu_item_game_save_state(void);
static void menu_item_0_launch(void);
static void menu_item_1_bug(void);
static void menu_item_2_not_found(void);
static void menu_item_3_found(void);
static void menu_item_4_step_back(void);
static void menu_show(void);
static void apply_glitches(void);
static void apply_found_glitches(void);
static void create_step_backup(void);
static void load_step_back_before_local(void);
static void current_search_end(void);
static void current_search_stop(void);
static void instructions_restore(void);
static void instructions_shuffle(void);
static void instructions_inversion(void);
static void show_notification(const char* s, uint8_t context);
static void game_reset(void);
static void game_save_state(void);
static void game_load_state(void);
static uint32_t xorshift(uint32_t* state);
static uint16_t get_rom_checksum(uint8_t* rom, uint32_t size);
static uint32_t virt_rom_to_real_rom_offset(uint32_t address);
static uint32_t real_rom_to_virt_rom_offset(uint32_t address);
static uint8_t save_glitch_to_file(uint32_t virt_address,
    uint32_t real_address, uint8_t intial_value, uint8_t mod_value);
static int16_t hook_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id);

void rg_init(uint8_t* rom_data, uint32_t size);
void rg_deinit(void);
void rg_menu_hide(void);
void rg_input_processing(void);
void rg_set_rom_in_mdx(void);
void rg_set_rom_is_byte_swapped(void);
void rg_set_rom_has_header(void);
void rg_set_rom_was_interleaved(void);
bool rg_get_menu_visible(void);
void rg_handle_pause_frame(void** pause_frame, int* pause_frame_width, int* pause_frame_height,
    const void* bitmap_data, int vwidth, int vheight, int bitmap_pitch, int bitmap_width);

#endif