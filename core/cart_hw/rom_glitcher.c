// rom_glitcher.c

// ROM Glitcher: Instruction Inverter
// 
// perfect_genius - glitcher idea, pav13 - implementation
// https://www.emu-land.net/forum/index.php/topic,88982.msg1652059.html#msg1652059

#define VIRT_TO_REAL true
#define REAL_TO_VIRT false

#include "rom_glitcher.h"
#include "rom_glitcher_menu.h"
#include "rom_glitcher_translation.h"
#include "rom_glitcher_opcode_valid.h"
#include "rom_glitcher_overlay.h"
#include <string.h>
#include <stdlib.h>

static void current_search_end(void);
static void create_step_backup(void);
static rom_glitcher_dhash64_t get_sreenshot_hash(rom_glitcher_bitmap_t* bmp);
static void remove_not_found_range_from_bug_range(uint32_t remove_size);
static void bug_range_comparison(const rom_glitcher_bug_range_t* src, rom_glitcher_bug_range_t* dst);
static void detect_bug(void);
static void remove_one_bug_from_rg_backup(uint32_t del_addr);
static void remove_one_bug_from_all_bug_range(uint32_t del_addr);
static void step4_back(void);
static bool cheats_file_parse(void);
static bool cheats_file_save(uint32_t real_address, 
    uint8_t initial_value, uint8_t mod_value, char* file_path, int file_path_size);
static void free_and_reset_memory(void);
static uint32_t address_offset_calculation(uint32_t address, bool direction);

rom_glitcher_main_t rg_main = {
    .glitch = NULL,
    .glitch_count = 0,
    .range_start = 0,
    .range_size = 0,
    .step_count = 0,
    .seed = 19881029,
    .range_size_adj = 0,
    .localizing = false,
    .launch_done = false,
    .init_done = false
};

rom_glitcher_found_glitches_t rg_found_glitches = {
    .count = 0,
    .enabled_count = 0,
    .current_page = 0,
    .total_pages = 0
};

rom_glitcher_input_replay_t rg_input_replay = {
    .record = false,
    .play = false,
    .play_count = 0,
    .length = 0
};

uint32_t rg_inst_allowed = 0b100; // default allowed only BEQ/BNE
static rom_glitcher_bug_range_t* bug_range = NULL;
rom_glitcher_bug_glitches_t rg_bug_glitches;
static uint16_t bug_range_count = 0;
static uint16_t bug_range_capacity = 0;
static rom_glitcher_bitmap_t rg_bitmap;
static rom_glitcher_main_t rg_backup[RG_MAX_BACKUP_SLOTS];
static uint8_t rg_backup_index = 0;
uint8_t rg_backup_count = 0;
static rom_glitcher_main_t rg_backup_before_local;
rom_glitcher_button_state_t rg_button_states[7] = { 0 }; // Key 0:Menu, 1:Prev, 2:Next, 3:Confirm/Found, 4:Cancel/Bug, 5:NotFound, 6:StepBack
int32_t rg_menu_button = RG_DISABLED_KEY;
bool rg_swap_buttons = false;
bool rg_found_glitches_modified = false;
bool rg_menu_visible = false;
bool rg_rom_in_mdx = false;
bool rg_rom_is_byte_swapped = false;
bool rg_rom_has_header = false;
bool rg_rom_was_deinterleaved = false;
uint8_t rg_pause_effect = 0;
uint16_t rg_fps = 60;
uint32_t rg_total_glitch_count = 0;
//char rg_log[1024];
static bool need_load_state = false;
static bool load_step_before_local = false;
bool rg_clear_bug_range = false;
static uint8_t game_state_buffer[STATE_SIZE];

static uint32_t xorshift(uint32_t* seed) {
    uint32_t x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (*seed = x);
}

// применение эффекта к последнему кадру игры при вызове меню глитчера
// (возможная будущая основа для "нового" меню)
static void handle_pause_frame(void) {
    static uint8_t old_pause_effect = 255;
    static uint16_t* pause_frame = NULL;

    if (rg_menu_visible) {
        if (old_pause_effect != rg_pause_effect) {
            if (pause_frame) {
                free(pause_frame);
                pause_frame = NULL;
            }
            old_pause_effect = rg_pause_effect;
        }

        if (rg_bitmap.data && !pause_frame) {
            const size_t frame_size = rg_bitmap.vwidth * rg_bitmap.vheight * 2;

            pause_frame = malloc(frame_size);
            if (!pause_frame)
                return;

            const uint16_t* src_frame = (const uint16_t*)rg_bitmap.data;
            const int src_pitch = rg_bitmap.pitch / 2;
            const int dst_pitch = rg_bitmap.vwidth;

            for (int y = 0; y < rg_bitmap.vheight; y++) {
                const uint16_t* src_row = &src_frame[y * src_pitch];
                uint16_t* dst_row = pause_frame + y * dst_pitch;

                for (int x = 0; x < rg_bitmap.vwidth; x++) {
                    uint16_t pixel = src_row[x];
                    uint8_t r = (pixel >> 11) & 0x1F;
                    uint8_t g = (pixel >> 5) & 0x3F;
                    uint8_t b = pixel & 0x1F;

                    switch (rg_pause_effect) {
                    case 0: // dark
                        r >>= 2; g >>= 2; b >>= 2;
                        break;

                    case 1: // red glitch
                        r = (r + 31) / 3;
                        g >>= 2; b >>= 2;
                        break;

                    case 2: { // sepia
                        uint8_t gray = (r + g + b) / 4;
                        r = gray;
                        g = (gray - (gray >> 2));
                        b = (gray >> 1);
                        break;
                    }

                    case 3: // yellow glitch
                        r = r * 3 / 4;
                        g = r;
                        b = 0;
                        break;

                    case 4: { // color static
                        uint8_t noise = xorshift(&rg_main.seed) % 25;
                        r = (r > noise) ? (r - noise) * 3 / 4 : 1;
                        g = (g > noise) ? (g - noise) * 3 / 4 : 1;
                        b = (b > noise) ? (b - noise) * 3 / 4 : 1;
                        break;
                    }

                    default: // no effect
                        break;
                    }

                    dst_row[x] = (r << 11) | (g << 5) | b;
                }
            }
            
            draw_overlay_progress_bar(pause_frame, dst_pitch, rg_bitmap.vwidth, rg_bitmap.vheight);
        }
        
        video_cb(pause_frame ? (uint8_t*)pause_frame : rg_bitmap.data, rg_bitmap.vwidth, rg_bitmap.vheight,
            rg_bitmap.vwidth * 2);
    }
    else {
        if (pause_frame) {
            free(pause_frame);
            pause_frame = NULL;
        }
    }
}

static void instructions_shuffle(void) {
#ifdef RANDOM_SEED
    rg_main.seed ^= m68k_get_reg(M68K_REG_PC);
    rg_main.seed ^= m68k_get_reg(M68K_REG_IR);
    rg_main.seed ^= m68k_get_reg(M68K_REG_D2);
#endif // RANDOM_SEED

    //равномерное перемешивание
    for (uint32_t i = 0; i < rg_main.glitch_count - 1; i++) {
        uint32_t j = i + (xorshift(&rg_main.seed) % (rg_main.glitch_count - i));
        rom_glitcher_glitch_t temp = rg_main.glitch[i];
        rg_main.glitch[i] = rg_main.glitch[j];
        rg_main.glitch[j] = temp;
    }
}

static void instructions_restore(void) {
    if (!rg_main.glitch)
        return;

    for (uint32_t i = rg_main.range_start; 
            i < rg_main.range_start + rg_main.range_size && i < rg_main.glitch_count; i++)
        rg_main.glitch[i].mod_value = rg_main.glitch[i].initial_value;
}

static void instructions_inversion(void) {
    if (!rg_main.glitch)
        return;
    
    for (uint32_t i = rg_main.range_start; 
        i < rg_main.range_start + rg_main.range_size && i < rg_main.glitch_count; 
        i++) {
        uint8_t value = rg_main.glitch[i].initial_value;

        // Bcc + Scc + DBcc + ADDQ/SUBQ
        if ((value >= 0x62 && value <= 0x6F) || (value >= 0x50 && value <= 0x5F)) {
            rg_main.glitch[i].mod_value = value ^ 0b00000001;
            continue;
        }
        
        // ADD/SUB, ADDX/SUBX, ADDA/SUBA
        if ((value >= 0x90 && value <= 0x9F) || (value >= 0xD0 && value <= 0xDF)) {
            rg_main.glitch[i].mod_value = value ^ 0b01000000;
            continue;
        }

        // ADDI/SUBI
        if (value == 0x04 || value == 0x06) {
            rg_main.glitch[i].mod_value = value ^ 0b00000010;
            continue;
        }
        
        rg_main.glitch[i].mod_value = value;
    }
}

// загрузка бэкапа до локализации и удаление найденной инструкции
static void load_step_back_before_local(void) {
    rom_glitcher_main_t* slot = &rg_backup_before_local;

    if (rg_main.glitch) {
        free(rg_main.glitch);
        rg_main.glitch = NULL;
    }

    rg_main = *slot;

    if (slot->glitch_count > 0 && slot->glitch) {
        rg_main.glitch = malloc(sizeof(rom_glitcher_glitch_t) * slot->glitch_count);
        if (rg_main.glitch) {
            memcpy(rg_main.glitch, slot->glitch, sizeof(rom_glitcher_glitch_t) * slot->glitch_count);

            for (uint32_t i = 0; i < rg_main.glitch_count; i++) {
                if (rg_main.glitch[i].address == rg_found_glitches.virt_address[rg_found_glitches.count - 1]) {
                    for (uint32_t j = i; j < rg_main.glitch_count - 1; j++)
                        rg_main.glitch[j] = rg_main.glitch[j + 1];

                    rg_main.glitch_count--;

                    rom_glitcher_glitch_t* tmp = realloc(rg_main.glitch,
                        sizeof(rom_glitcher_glitch_t) * rg_main.glitch_count);

                    if (tmp || rg_main.glitch_count == 0)
                        rg_main.glitch = tmp;

                    break;
                }
            }

            rg_msg(RG_MSG_INFO, TR(RG_TR_CONTINUE_SEARCH));
        }
        else {
            rg_main.glitch = NULL;
            rg_main.glitch_count = 0;
            rg_msg(RG_MSG_ERROR, "%s [M01]", TR(RG_TR_ERROR_MEMORY));
        }
    }
    else {
        rg_main.glitch = NULL;
        rg_msg(RG_MSG_ERROR, "%s [B01]", TR(RG_TR_ERROR_BACKUP));
    }

    rg_game_reset();
}

void rg_launch_glitcher(void) {
    if (rg_main.init_done) {
        rg_game_save_state();
        rg_main.launch_done = true;
        rg_menu.search.selected_index = 0;
        rg_menu.options.selected_index = 0;
        rg_menu.current = &rg_menu.search;
        create_step_backup();
        instructions_shuffle();
        rg_main.step_count++;
        instructions_inversion();
        rg_input_replay.record = true;
        rg_input_replay.play = false;
        rg_input_replay.length = 0;
    }
    else
        rg_msg(RG_MSG_ERROR, "%s [C01]", TR(RG_TR_ERROR_COMMON_1));
}

static void step1_bug(void) {
    if (rg_bug_glitches.count) {
        rg_bug_glitches.count--;
        remove_one_bug_from_rg_backup(rg_bug_glitches.address[rg_bug_glitches.count]);

        // здесь может быть bug_glitches.count++
        remove_one_bug_from_all_bug_range(rg_bug_glitches.address[rg_bug_glitches.count]);

        rg_msg(RG_MSG_DEBUG, "Ликуем :-) [баг-инструкция удалена]");

        if (rg_bug_glitches.count) {
            rg_msg(RG_MSG_DEBUG, "Отображена следующая баг-инструкция");
            rg_game_reset();
        }
        else {
            step4_back();
        }

        return;
    }

    create_step_backup();
    detect_bug(); // здесь может быть bug_glitches.count++
    instructions_restore();

    if (rg_bug_glitches.count) {
        rg_game_reset();
        return;
    }

    if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
        if (rg_main.range_size == 1) {
            if (rg_main.localizing)
                rg_msg(RG_MSG_ERROR, "%s [G01]", TR(RG_TR_ERROR_ADDRESS));
            else
                rg_msg(RG_MSG_ERROR, "%s [G02]", TR(RG_TR_ERROR_ADDRESS));

            current_search_end();
            return;
        }
        else {
            rg_main.range_start = 0;
            //rg_main.range_size -= rg_main.range_size / 2;
            if (rg_main.range_size_adj < 2)
                rg_main.range_size -= rg_main.range_size / 2;
            else
                rg_main.range_size *= 3;

            rg_main.range_size_adj = (rg_main.range_size_adj + 1) % 3;

            if (!rg_main.localizing)
                instructions_shuffle();
        }
    }
    else
        rg_main.range_start += rg_main.range_size;

    rg_main.step_count++;
    instructions_inversion();
    rg_game_reset();
    
}

static void step2_not_found(void) {
    if (rg_bug_glitches.count) {
        rg_bug_glitches.count--;
        remove_one_bug_from_rg_backup(rg_bug_glitches.address[rg_bug_glitches.count]);

        // здесь может быть bug_glitches.count++
        remove_one_bug_from_all_bug_range(rg_bug_glitches.address[rg_bug_glitches.count]);

        rg_msg(RG_MSG_DEBUG, "Плачем :`( [это не баг-инструкция]");

        if (rg_bug_glitches.count) {
            rg_msg(RG_MSG_DEBUG, "Отображена следующая баг-инструкция");
            rg_game_reset();
        }
        else
            step4_back();

        return;
    }

    create_step_backup();
    instructions_restore();

    if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
        if (rg_main.range_size == 1 || rg_main.range_start == 0) {
            if (rg_main.localizing)
                rg_msg(RG_MSG_ERROR, "%s [G03]", TR(RG_TR_ERROR_ADDRESS));
            else
                rg_msg(RG_MSG_ERROR, "%s [G04]", TR(RG_TR_ERROR_ADDRESS));

            current_search_end();
            return;
        }
        else {
            rg_main.glitch_count = rg_main.range_start;
            rg_main.range_start = 0;
            //rg_main.range_size -= rg_main.range_size / 2;
            if (rg_main.range_size_adj < 2)
                rg_main.range_size -= rg_main.range_size / 2;
            else
                rg_main.range_size *= 3;

            rg_main.range_size_adj = (rg_main.range_size_adj + 1) % 3;


            if (!rg_main.localizing)
                instructions_shuffle();
        }
    }
    else {
        uint32_t remove_size = rg_main.range_size;
        if (rg_main.range_start + remove_size > rg_main.glitch_count)
            remove_size = rg_main.glitch_count - rg_main.range_start;

        // здесь может быть bug_glitches.count++
        // УЖЕ НЕТ !!!
        remove_not_found_range_from_bug_range(remove_size);

        if (remove_size > 0) {
            memmove(&rg_main.glitch[rg_main.range_start],
                &rg_main.glitch[rg_main.range_start + remove_size],
                (rg_main.glitch_count - rg_main.range_start - remove_size) * sizeof(rom_glitcher_glitch_t));
            rg_main.glitch_count -= remove_size;
        }

        if (rg_main.localizing)
            rg_main.range_size -= rg_main.glitch_count / 2;
    }

    rg_main.step_count++;
    instructions_inversion();
    
    if (rg_bug_glitches.count)
        create_step_backup();

    rg_game_reset();
}

static void step3_found(void) {
    if (rg_bug_glitches.count) {
        rg_bug_glitches.count--;
        remove_one_bug_from_all_bug_range(rg_bug_glitches.address[rg_bug_glitches.count]); // здесь может быть bug_glitches.count++

        rg_msg(RG_MSG_DEBUG, "Плачем и ликуем 8-| [баг-инструкция оказалась искомой]");

        if (rg_bug_glitches.count) {
            rg_msg(RG_MSG_DEBUG, "Отображена следующая баг-инструкция");
            rg_game_reset();
        }
        else {
            step4_back();
        }
        
        return;
    }

    if (!rg_main.localizing) {
        // отдельный бэкап-чекпоинт перед локализацией
        rom_glitcher_main_t* slot = &rg_backup_before_local;

        if (slot->glitch) {
            free(slot->glitch);
            slot->glitch = NULL;
        }

        *slot = rg_main;

        if (rg_main.glitch_count > 0 && rg_main.glitch) {
            slot->glitch = malloc(sizeof(rom_glitcher_glitch_t) * rg_main.glitch_count);
            if (slot->glitch)
                memcpy(slot->glitch, rg_main.glitch, sizeof(rom_glitcher_glitch_t) * rg_main.glitch_count);
            else {
                slot->glitch = NULL;
                slot->glitch_count = 0;
            }
        }
        else
            slot->glitch = NULL;

        rg_main.localizing = true;
    }

    create_step_backup();

    if (rg_main.range_size == 1 || rg_main.glitch_count == 1) {
        // адрес найден

        // поиск дубликата в уже найденных адресах
        for (int i = 0; i < rg_found_glitches.count; i++) {
            if (rg_main.glitch[0].address == rg_found_glitches.virt_address[i]) {
                current_search_end();
                return;
            }
        }

        // выбор слота для добавления адреса при превышении общего количества
        if (rg_found_glitches.count >= RG_MAX_FOUND_GLITCH_SLOTS) {
            for (int i = 1; i < RG_MAX_FOUND_GLITCH_SLOTS; i++) {
                rg_found_glitches.virt_address[i - 1] = rg_found_glitches.virt_address[i];
                rg_found_glitches.initial_value[i - 1] = rg_found_glitches.initial_value[i];
                rg_found_glitches.real_address[i - 1] = rg_found_glitches.real_address[i];
                rg_found_glitches.enabled[i - 1] = rg_found_glitches.enabled[i];
            }

            rg_found_glitches.count = RG_MAX_FOUND_GLITCH_SLOTS - 1;
            rg_msg(RG_MSG_INFO, "%s [C02]", TR(RG_TR_ERROR_COMMON_2)); // full slots
        }

        rg_found_glitches.initial_value[rg_found_glitches.count] = rg_main.glitch[0].initial_value;
        rg_found_glitches.mod_value[rg_found_glitches.count] = rg_main.glitch[0].mod_value;
        rg_found_glitches.enabled[rg_found_glitches.count] = true;
        rg_found_glitches.virt_address[rg_found_glitches.count] = rg_main.glitch[0].address;
        rg_found_glitches.real_address[rg_found_glitches.count] = address_offset_calculation(rg_found_glitches.virt_address[rg_found_glitches.count], VIRT_TO_REAL);
        char file_path[128] = { 0 };
        cheats_file_save(rg_found_glitches.real_address[rg_found_glitches.count], rg_found_glitches.initial_value[rg_found_glitches.count], rg_found_glitches.mod_value[rg_found_glitches.count], file_path, sizeof(file_path));

        rg_found_glitches.count++;
        rg_found_glitches.total_pages =
            (rg_found_glitches.count + RG_FOUND_GLITCH_PER_PAGE - 1) / RG_FOUND_GLITCH_PER_PAGE;

        rg_msg(RG_MSG_FOUND, "%s\n%s %u. %s %u. ROM '0x%06X' (0x%02X->0x%02X)", file_path,
            TR(RG_TR_GLITCH), rg_found_glitches.count,
            TR(RG_TR_STEP), rg_main.step_count, 
            rg_found_glitches.real_address[rg_found_glitches.count - 1],
            rg_found_glitches.initial_value[rg_found_glitches.count - 1], 
            rg_found_glitches.mod_value[rg_found_glitches.count - 1]);
        current_search_end();
    }
    else {
        instructions_restore();

        if (rg_main.range_start > 0 && rg_main.range_start < rg_main.glitch_count) {
            uint32_t remove_size = rg_main.range_start;
            memmove(rg_main.glitch,
                &rg_main.glitch[rg_main.range_start],
                (rg_main.glitch_count - rg_main.range_start) * sizeof(rom_glitcher_glitch_t));
            rg_main.glitch_count -= remove_size;
        }

        rg_main.range_start = 0;

        if (rg_main.range_size < rg_main.glitch_count)
            rg_main.glitch_count = rg_main.range_size;
        else if (rg_main.range_size > rg_main.glitch_count)
            rg_main.range_size = rg_main.glitch_count;

        rg_main.range_size -= rg_main.range_size / 2;
        rg_main.step_count++;
        instructions_inversion();
        rg_game_reset();
    }
}

static void step4_back(void) {
    if (rg_bug_glitches.count) {
        rg_msg(RG_MSG_DEBUG, "При выводе одного бага шаг Назад не доступен");
        return;
    }

    if (rg_backup_count == 0) {
        rg_msg(RG_MSG_INFO, "%s [B02]", TR(RG_TR_ERROR_BACKUP));
        return;
    }

    rg_backup_index = (rg_backup_index - 1 + RG_MAX_BACKUP_SLOTS) % RG_MAX_BACKUP_SLOTS;
    rg_backup_count--;

    rom_glitcher_main_t* slot = &rg_backup[rg_backup_index];

    if (rg_main.glitch) {
        free(rg_main.glitch);
        rg_main.glitch = NULL;
    }

    rg_main = *slot;

    if (slot->glitch_count > 0 && slot->glitch) {
        rg_main.glitch = malloc(sizeof(rom_glitcher_glitch_t) * slot->glitch_count);
        if (rg_main.glitch) {
            memcpy(rg_main.glitch, slot->glitch, sizeof(rom_glitcher_glitch_t) * slot->glitch_count);
        }
        else {
            rg_main.glitch = NULL;
            rg_main.glitch_count = 0;
            rg_msg(RG_MSG_ERROR, "%s [M02]", TR(RG_TR_ERROR_MEMORY));
        }
    }
    else {
        rg_main.glitch = NULL;
        rg_msg(RG_MSG_ERROR, "%s [B03]", TR(RG_TR_ERROR_BACKUP));
    }

    rg_game_reset();
}

static int16_t hook_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (device == RETRO_DEVICE_JOYPAD) {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return rg_input_replay.hook_mask[port];
        if (id <= RETRO_DEVICE_ID_JOYPAD_R3)
            return (rg_input_replay.hook_mask[port] >> id) & 1;
    }
    return 0;
}

// опрос кнопок геймпада и обработка ввода
void rg_handle_input(const t_bitmap* bitmap, const int* vwidth, const int* vheight) {
    if (!rg_menu_visible && (bitmap || bitmap->data)) {
        rg_bitmap.data = bitmap->data;
        rg_bitmap.width = bitmap->width;
        rg_bitmap.height = bitmap->height;
        rg_bitmap.pitch = bitmap->pitch;
        rg_bitmap.vwidth = *vwidth;
        rg_bitmap.vheight = *vheight;
    }

    handle_pause_frame();

    if (!input_poll_cb || !input_state_cb)
        return;
    
    if (need_load_state) {
        need_load_state = false;

        if(rg_main.launch_done)
            rg_game_load_state();

        if (rg_input_replay.length > 0) {
            rg_input_replay.play = true;
            rg_input_replay.play_count = 0;
            rg_input_replay.input_cb_copy = input_state_cb;
            input_state_cb = hook_input_state_cb;
        }

        return;
    }

    input_poll_cb();

    int16_t current_mask[RG_MAX_REPLAY_GAMEPADS] = { 0 };

    // ----------------- rg_input_replay.record -----------------
    if (rg_input_replay.record) {
        if (rg_input_replay.length >= RG_MAX_REPLAY_FRAMES) {
            rg_input_replay.record = false;
            rg_game_reset();
            return;
        }

        if (libretro_supports_bitmasks)
            for (int port = 0; port < RG_MAX_REPLAY_GAMEPADS; port++) {
                current_mask[port] = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
                rg_input_replay.sequence[port][rg_input_replay.length++] = current_mask[port];
            }
        else
            for (int port = 0; port < RG_MAX_REPLAY_GAMEPADS; port++)
                for (int id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
                    if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, id)) {
                        current_mask[port] |= (1 << id);
                        rg_input_replay.sequence[port][rg_input_replay.length++] = current_mask[port];
                    }
        
        rg_msg(RG_MSG_REPLAY_REC, "%u", 100 - (rg_input_replay.length * 100) / RG_MAX_REPLAY_FRAMES);
    }
    // ----------------- rg_input_replay.play -----------------
    else if (rg_input_replay.play && rg_input_replay.length > 0) {
        if (libretro_supports_bitmasks)
            current_mask[0] = rg_input_replay.input_cb_copy(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        else
            for (int id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
                if (rg_input_replay.input_cb_copy(0, RETRO_DEVICE_JOYPAD, 0, id))
                    current_mask[0] |= (1 << id);
        
        if (rg_input_replay.play_count < rg_input_replay.length) {
            for (int port = 0; port < RG_MAX_REPLAY_GAMEPADS; port++)
                rg_input_replay.hook_mask[port] = rg_input_replay.sequence[port][rg_input_replay.play_count++];
            
            rg_msg(RG_MSG_REPLAY_PLAY, "%u", (rg_input_replay.play_count * 100) / rg_input_replay.length);
        }
        else {
            rg_input_replay.play = false;
            input_state_cb = rg_input_replay.input_cb_copy;
            rg_menu_visible = true;
            rg_menu.current = &rg_menu.search;
            rg_menu.current->selected_index = 0;
        }
    }
    // ----------------- normal -----------------
    else {
        if (libretro_supports_bitmasks)
            current_mask[0] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        else
            for (int id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
                if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id))
                    current_mask[0] |= (1 << id);
    }

    bool menu_key = ((current_mask[0] & rg_menu_button) == rg_menu_button);

    // ----------------- menu_key -----------------
    if (!menu_key && rg_button_states[0].was_pressed && !rg_button_states[0].is_processed) {
        if (rg_input_replay.record) {
            rg_input_replay.record = false;
            rg_game_reset();
        }
        else if (rg_input_replay.play) {
            rg_input_replay.play = false;
            input_state_cb = rg_input_replay.input_cb_copy;
            rg_menu_visible = true;
            rg_menu.current = &rg_menu.search;
            rg_menu.current->selected_index = 0;
        }
        else {
            rg_menu_visible = !rg_menu_visible;
            if (rg_menu_visible) {
                rg_menu.current = rg_main.launch_done ? &rg_menu.search : &rg_menu.launch;
                rg_menu.current->selected_index = 0;
            }
            else {
                rg_menu_hide();

                if (load_step_before_local) {
                    load_step_before_local = false;
                    rg_found_glitches_modified = false;
                    load_step_back_before_local();
                }
                else if (rg_found_glitches_modified) {
                    rg_found_glitches_modified = false;
                    rg_msg(RG_MSG_INFO, TR(RG_TR_ADDRESS_CHANGED));
                    rg_game_reset();
                }
            }
        }

        rg_button_states[0].is_processed = true;
    }
    rg_button_states[0].was_pressed = menu_key;
    if (menu_key) rg_button_states[0].is_processed = false;

    if (rg_menu_visible || rg_input_replay.play) {
        bool prev_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_UP) & 1;
        bool next_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_DOWN) & 1;
        bool confirm_found_key = (current_mask[0] >>
            (rg_swap_buttons ? RETRO_DEVICE_ID_JOYPAD_B : RETRO_DEVICE_ID_JOYPAD_A)) & 1;
        bool cancel_bug_key = (current_mask[0] >>
            (rg_swap_buttons ? RETRO_DEVICE_ID_JOYPAD_A : RETRO_DEVICE_ID_JOYPAD_B)) & 1;
        bool not_found_key = (current_mask[0] >>
            (rg_swap_buttons ? RETRO_DEVICE_ID_JOYPAD_Y : RETRO_DEVICE_ID_JOYPAD_X)) & 1;
        bool step_back_key = (current_mask[0] >>
            (rg_swap_buttons ? RETRO_DEVICE_ID_JOYPAD_X : RETRO_DEVICE_ID_JOYPAD_Y)) & 1;

        // ----------------- prev_key -----------------
        if (!prev_key && rg_button_states[1].was_pressed && !rg_button_states[1].is_processed) {
            if (rg_input_replay.play)
                ;
            else if (rg_menu.current) {
                rg_menu.current->selected_index =
                    (rg_menu.current->selected_index - 1 + rg_menu.current->item_count) % rg_menu.current->item_count;
            }
            rg_button_states[1].is_processed = true;
        }
        rg_button_states[1].was_pressed = prev_key;
        if (prev_key) rg_button_states[1].is_processed = false;

        // ----------------- next_key -----------------
        if (!next_key && rg_button_states[2].was_pressed && !rg_button_states[2].is_processed) {
            if (rg_input_replay.play)
                ;
            else if (rg_menu.current) {
                rg_menu.current->selected_index = (rg_menu.current->selected_index + 1) % rg_menu.current->item_count;
            }
            rg_button_states[2].is_processed = true;
        }
        rg_button_states[2].was_pressed = next_key;
        if (next_key) rg_button_states[2].is_processed = false;

        // ----------------- confirm_found_key -----------------
        if (!confirm_found_key && rg_button_states[3].was_pressed && !rg_button_states[3].is_processed) {
            if (rg_input_replay.play) {
                rg_input_replay.play = false;
                input_state_cb = rg_input_replay.input_cb_copy;
                step3_found();
            }
            else if (rg_menu.current == &rg_menu.search && rg_menu.current->selected_index == 0)
                step3_found();
            else if (rg_menu.current) {
                rom_glitcher_menu_item_t* item = &rg_menu.current->items[rg_menu.current->selected_index];
                if (item->action)
                    item->action();
            }
            rg_button_states[3].is_processed = true;
        }
        rg_button_states[3].was_pressed = confirm_found_key;
        if (confirm_found_key) rg_button_states[3].is_processed = false;

        // ----------------- cancel_bug_key -----------------
        if (!cancel_bug_key && rg_button_states[4].was_pressed && !rg_button_states[4].is_processed) {
            if (rg_input_replay.play) {
                rg_input_replay.play = false;
                input_state_cb = rg_input_replay.input_cb_copy;
                step1_bug();
            }
            else if (rg_menu.current == &rg_menu.inst_add_sub || rg_menu.current == &rg_menu.inst_bcc ||
                rg_menu.current == &rg_menu.inst_scc || rg_menu.current == &rg_menu.inst_dbcc) {
                rg_menu.current = &rg_menu.command;
            }
            else if (rg_menu.current != &rg_menu.search && rg_menu.current != &rg_menu.launch) {
                rg_menu.current = rg_main.launch_done ? &rg_menu.search : &rg_menu.launch;
                if (load_step_before_local)
                    rg_menu.current->selected_index = 1;
            }
            else if (rg_menu.current == &rg_menu.search && rg_menu.current->selected_index == 0)
                step1_bug();
            else {
                rg_menu_hide();

                if (load_step_before_local) {
                    load_step_before_local = false;
                    rg_found_glitches_modified = false;
                    load_step_back_before_local();
                }
                else if (rg_found_glitches_modified) {
                    rg_found_glitches_modified = false;
                    rg_msg(RG_MSG_INFO, TR(RG_TR_ADDRESS_CHANGED));
                    rg_game_reset();
                }
            }

            rg_button_states[4].is_processed = true;
        }
        rg_button_states[4].was_pressed = cancel_bug_key;
        if (cancel_bug_key) rg_button_states[4].is_processed = false;
        
        // ----------------- not_found_key -----------------
        if (!not_found_key && rg_button_states[5].was_pressed && !rg_button_states[5].is_processed) {
            if (rg_input_replay.play) {
                rg_input_replay.play = false;
                input_state_cb = rg_input_replay.input_cb_copy;
                step2_not_found();
            }
            else if (rg_menu.current == &rg_menu.search && rg_menu.current->selected_index == 0)
                step2_not_found();

            rg_button_states[5].is_processed = true;
        }
        rg_button_states[5].was_pressed = not_found_key;
        if (not_found_key) rg_button_states[5].is_processed = false;

        // ----------------- step_back_key -----------------
        if (!step_back_key && rg_button_states[6].was_pressed && !rg_button_states[6].is_processed) {
            if (rg_input_replay.play) {
                rg_input_replay.play = false;
                input_state_cb = rg_input_replay.input_cb_copy;
                step4_back();
            }
            else if (rg_menu.current == &rg_menu.search && rg_menu.current->selected_index == 0)
                step4_back();

            rg_button_states[6].is_processed = true;
        }
        rg_button_states[6].was_pressed = step_back_key;
        if (step_back_key) rg_button_states[6].is_processed = false;
    }
    else
        for (int i = 1; i < ARRAY_SIZE(rg_button_states); i++)
            rg_button_states[i].was_pressed = rg_button_states[i].is_processed = false;
    
    rg_menu_show();
}

static uint16_t get_rom_checksum(uint8_t* rom, uint32_t size) {
    uint16_t checksum = 0;

    for (int i = 0; i < size; i += 2) {
        uint8_t next_byte = (i + 1 < size) ? rom[i + 1] : 0;
        checksum += ((rom[i] << 8) + next_byte);
    }

    return checksum;
}

// применение к ROM глитчей из списка найденных
static void apply_found_glitches(void) {
    rg_found_glitches.enabled_count = 0;

    for (int i = 0; i < rg_found_glitches.count; i++) {
        if (rg_found_glitches.enabled[i]) {
            cart.rom[rg_found_glitches.virt_address[i]] = rg_found_glitches.mod_value[i];
            rg_found_glitches.enabled_count++;
        }
        else
            cart.rom[rg_found_glitches.virt_address[i]] = rg_found_glitches.initial_value[i];
    }
}

// изменение ROM при отсеивании кандидатов
static void apply_glitches(void) {
    for (uint32_t i = 0; i < rg_main.glitch_count; i++)
        cart.rom[rg_main.glitch[i].address] = rg_main.glitch[i].mod_value;
    
    apply_found_glitches();

    if (rg_bug_glitches.count)
        cart.rom[rg_bug_glitches.address[rg_bug_glitches.count - 1]] = rg_bug_glitches.mod_value[rg_bug_glitches.count - 1];

    // если заголовок не был удалён эмулятором
    if (!rg_rom_has_header) {
        // пересчёт контрольной суммы в заголовке
        uint16_t real_checksum = get_rom_checksum(((uint8*)cart.rom) + RG_ROM_HEADER_SIZE, cart.romsize - RG_ROM_HEADER_SIZE);
        cart.rom[0x18E] = (real_checksum >> 8) & 0xFF;
        cart.rom[0x18F] = real_checksum & 0xFF;
    }

    need_load_state = true;
}
 
// поиск инструкций в ROM
static bool instructions_scan_rom(uint8_t* rom_data, uint32_t rom_size, uint16_t* prev_found_count) {
    int32_t trim = rg_rom_has_header ? 0 : RG_ROM_HEADER_SIZE;
    uint32_t capacity = 10000;
    rg_main.glitch_count = 0;
    rg_main.glitch = malloc(capacity * sizeof(rom_glitcher_glitch_t));

#ifdef COMPRESSED_OPCODE_TABLE
    m68k_opcode_valid_init();
#endif // COMPRESSED_OPCODE_TABLE
    
    // Читаем нормализованный ROM
    for (uint32_t byte_addr = trim; byte_addr + 1 < rom_size; byte_addr += 2) {
        bool next_checks = false;
        uint8_t high_byte = rom_data[byte_addr];
        uint16_t opcode = (high_byte << 8) | rom_data[byte_addr + 1];

    // --- Bcc инструкции ---
        if ((rg_inst_allowed & 0x000000FF) && 
            (high_byte >= 0x62 && high_byte <= 0x6f)) {
            // Проверка бита разрешения поиска конкретной пары Bcc инструкций
            if (!(rg_inst_allowed & (1 << ((high_byte - 0x62) >> 1))))
                continue;

            // Проверка целевого адреса на чётность и попадание в ROM
            int8_t offset8 = rom_data[byte_addr + 1];
            int32_t target_addr = 0;

            if (offset8 != 0) { // Короткое смещение
                // Проверка короткого смещения на валидность
#ifdef COMPRESSED_OPCODE_TABLE
                if (!m68k_opcode_valid(opcode))
                    continue;
#else
                if (!m68k_opcode_valid_table[opcode])
                    continue;
#endif // COMPRESSED_OPCODE_TABLE

                // Проверка опкода после предпологаемой Bcc инструкции
                if (byte_addr + 3 >= rom_size)
                    continue;

#ifdef COMPRESSED_OPCODE_TABLE
                if (!m68k_opcode_valid((rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3]))
                    continue;
#else
                if (!m68k_opcode_valid_table[(rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3]])
                    continue;
#endif // COMPRESSED_OPCODE_TABLE

                target_addr = byte_addr + 2 + offset8;
            }
            else { // Длинное смещение
                // Проверка опкода после предпологаемой Bcc инструкции
                if (byte_addr + 5 >= rom_size)
                    continue;

#ifdef COMPRESSED_OPCODE_TABLE
                if (!m68k_opcode_valid((rom_data[byte_addr + 4] << 8) | rom_data[byte_addr + 5]))
                    continue;
#else
                if (!m68k_opcode_valid_table[(rom_data[byte_addr + 4] << 8) | rom_data[byte_addr + 5]])
                    continue;
#endif // COMPRESSED_OPCODE_TABLE

                int16_t offset16 = (rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3];

                if (offset16 % 2 || offset16 == 0 || offset16 == -2)
                    continue;

                target_addr = byte_addr + 2 + offset16;
            }

            if (target_addr < trim || (target_addr + 1) >= (int32_t)rom_size)
                continue;

            // Проверка опкода по целевому адресу на валидность
            opcode = (rom_data[target_addr] << 8) | rom_data[target_addr + 1];
            next_checks = true;
        }
        
    // --- DBcc инструкции ---
        else if ((rg_inst_allowed & 0x00FF0000) &&
            ((opcode & 0b1111000011111000) == 0b0101000011001000)) {
            // Проверка бита разрешения поиска конкретной пары DBcc инструкций
            if (!(rg_inst_allowed & (1 << (((opcode >> 8) & 0x0F) + 16))))
                continue;

            next_checks = true;
        }

    // --- Scc инструкции ---
        else if ((rg_inst_allowed & 0x0000FF00) &&
            ((opcode & 0b1111000011000000) == 0b0101000011000000)) {
            // Проверка бита разрешения поиска конкретной пары Scc инструкций
            if (!(rg_inst_allowed & (1 << (((opcode >> 8) & 0x0F) + 8))))
                continue;

            next_checks = true;
        }

    // --- ADDQ/SUBQ инструкции ---
        else if ((rg_inst_allowed & (1 << INST_ADDQ_SUBQ)) &&
            (high_byte >= 0x50 && high_byte <= 0x5F)     // ADDQ/SUBQ
            ) {
            if (((opcode & 0b1111000100000000) == 0b0101000000000000) ||	// ADDQ
                ((opcode & 0b1111000100000000) == 0b0101000100000000)) {    // SUBQ
                next_checks = true;
            }

            if (!next_checks)
                continue;

#ifdef COMPRESSED_OPCODE_TABLE
            if (!m68k_opcode_valid(opcode))
                continue;
#else
            if (!m68k_opcode_valid_table[opcode])
                continue;
#endif // COMPRESSED_OPCODE_TABLE

            // Проверка опкода после предпологаемой ADDQ/SUBQ инструкции
            if (byte_addr + 3 >= rom_size)
                continue;

            opcode = (rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3];
        }

    // --- ADD/SUB, ADDX/SUBX, ADDA/SUBA, ADDI/SUBI инструкции ---
        else if ((rg_inst_allowed & 0x0F000000) &&
            ((high_byte >= 0x90 && high_byte <= 0x9F) ||    // SUB/SUBX/SUBA
            (high_byte >= 0xD0 && high_byte <= 0xDF) ||     // ADD/ADDX/ADDA
            (high_byte == 0x04) || (high_byte == 0x06))     // ADDI/SUBI
            ) {
            //opcode = (rom_data[byte_addr] << 8) | rom_data[byte_addr + 1];

            if (((opcode & 0b1111000011000000) == 0b1101000011000000) ||        // ADDA
                ((opcode & 0b1111000011000000) == 0b1001000011000000)) {        // SUBA
                if (rg_inst_allowed & (1 << INST_ADDA_SUBA))
                    next_checks = true;
            }
            else if (((opcode & 0b1111000100110000) == 0b1101000100000000) ||   // ADDX
                ((opcode & 0b1111000100110000) == 0b1001000100000000)) {        // SUBX
                if (rg_inst_allowed & (1 << INST_ADDX_SUBX))
                    next_checks = true;
            }
            else if (((opcode & 0b1111000000000000) == 0b1101000000000000) ||   // ADD
                ((opcode & 0b1111000000000000) == 0b1001000000000000)) {        // SUB
                if (rg_inst_allowed & (1 << INST_ADD_SUB))
                    next_checks = true;
            }
            else if (((opcode & 0b1111111100000000) == 0b0000011000000000) ||   // ADDI
                ((opcode & 0b1111111100000000) == 0b0000010000000000)) {        // SUBI
                if (rg_inst_allowed & (1 << INST_ADDI_SUBI))
                    next_checks = true;
            }
            
            if (!next_checks)
                continue;

#ifdef COMPRESSED_OPCODE_TABLE
            if (!m68k_opcode_valid(opcode))
                continue;
#else
            if (!m68k_opcode_valid_table[opcode])
                continue;
#endif // COMPRESSED_OPCODE_TABLE

            // Проверка опкода после предпологаемой ADD*/SUB* инструкции
            if (byte_addr + 3 >= rom_size)
                continue;

            opcode = (rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3];
        }

        // если прошли предыдущие проверки
        if (next_checks) {
#ifdef COMPRESSED_OPCODE_TABLE
            if (!m68k_opcode_valid(opcode))
                continue;
#else
            if (!m68k_opcode_valid_table[opcode])
                continue;
#endif // COMPRESSED_OPCODE_TABLE 

            // Пропуск ранее сохранённой найденной инструкции
            bool prev_found = false;
            for (int i = 0; i < rg_found_glitches.count; i++) {
                if (byte_addr == rg_found_glitches.virt_address[i]) {
                    prev_found = true;
                    (*prev_found_count)++;
                    break;
                }
            }

            if (prev_found)
                continue;

            // Правдоподобная инструкция
            if (rg_main.glitch_count >= capacity) {
                uint32_t temp_capacity = capacity * 2;
                rom_glitcher_glitch_t* temp_glitches = realloc(rg_main.glitch, temp_capacity * sizeof(rom_glitcher_glitch_t));

                if (!temp_glitches) {
                    free(rg_main.glitch);
                    rg_main.glitch = NULL;
                    rg_msg(RG_MSG_ERROR, "%s [M03]", TR(RG_TR_ERROR_MEMORY));
                    return false;
                }

                rg_main.glitch = temp_glitches;
                capacity = temp_capacity;
            }

            rg_main.glitch[rg_main.glitch_count].address = byte_addr;
            rg_main.glitch[rg_main.glitch_count].initial_value = high_byte;
            rg_main.glitch[rg_main.glitch_count].mod_value = high_byte;
            rg_main.glitch_count++;
        }
    }

    if (rg_main.glitch_count > 0) {
        if (capacity > rg_main.glitch_count) {
            rom_glitcher_glitch_t* temp_glitches = 
                realloc(rg_main.glitch, rg_main.glitch_count * sizeof(rom_glitcher_glitch_t));

            if (temp_glitches)
                rg_main.glitch = temp_glitches;
        }

        return true;
    }

    if (rg_main.glitch) {
        free(rg_main.glitch);
        rg_main.glitch = NULL;
    }

    return false;
}

// инициализация глитчера
void rg_init(uint8_t* rom_data, uint32_t rom_size) {
    if (rg_main.init_done) {
        apply_glitches();
        return;
    }

    free_and_reset_memory();
    rg_get_language();
    cheats_file_parse();
    apply_found_glitches();

    // если заголовок не был удалён эмулятором
    if (!rg_rom_has_header) {
        uint16_t header_checksum = (cart.rom[0x18E] << 8) | cart.rom[0x18F];
        uint16_t real_checksum = get_rom_checksum(((uint8*)cart.rom) + RG_ROM_HEADER_SIZE, cart.romsize - RG_ROM_HEADER_SIZE);
        // обновление контрольной суммы в заголовке
        if (header_checksum != real_checksum) {
            cart.rom[0x18E] = (real_checksum >> 8) & 0xFF;
            cart.rom[0x18F] = real_checksum & 0xFF;
        }
    }

    uint16_t prev_found_count = 0;
    rg_main.init_done = instructions_scan_rom(rom_data, rom_size, &prev_found_count);

#ifndef RANDOM_SEED
    rg_main.seed = 19881029;
#endif // RANDOM_SEED
    struct retro_system_av_info av_info;
    retro_get_system_av_info(&av_info);
    rg_fps = (uint16_t)(av_info.timing.fps + 0.5);
    rg_fps = rg_fps > 0 ? rg_fps : 60;
    need_load_state = false;
    load_step_before_local = false;
    rg_found_glitches_modified = false; 
    rg_found_glitches.current_page = 0;
    rg_found_glitches.total_pages = rg_found_glitches.count > 0 ?
        ((rg_found_glitches.count + RG_FOUND_GLITCH_PER_PAGE - 1) / RG_FOUND_GLITCH_PER_PAGE)
        : 1;
    rg_input_replay.record = false;
    rg_input_replay.play = false;
    rg_input_replay.length = 0;
    rg_main.step_count = 0;
    rg_main.localizing = false;
    rg_main.launch_done = false;
    rg_total_glitch_count = rg_main.glitch_count;
    rg_main.range_size_adj = 0;
    rg_main.range_start = 0;
    //rg_main.range_size = (rg_main.glitch_count + 31) / 32; // 3% от всех кандидатов
    rg_main.range_size = (rg_main.glitch_count + 15) / 16; // 6% от всех кандидатов
     
    rg_msg(rg_main.init_done ? RG_MSG_INFO : RG_MSG_ERROR,
        prev_found_count > 0 ? "%s %u (%s %u)" : "%s %u",
        TR(RG_TR_CANDIDATES),
        rg_main.glitch_count,
        prev_found_count > 0 ? TR(RG_TR_FOUND) : "",
        prev_found_count);
}

// сохранение предыдущего состояния отсеивания кандидатов
static void create_step_backup(void) {
    rom_glitcher_main_t* slot = &rg_backup[rg_backup_index];

    if (slot->glitch) {
        free(slot->glitch);
        slot->glitch = NULL;
    }

    *slot = rg_main;

    if (rg_main.glitch_count > 0 && rg_main.glitch) {
        slot->glitch = malloc(sizeof(rom_glitcher_glitch_t) * rg_main.glitch_count);
        if (slot->glitch) {
            memcpy(slot->glitch, rg_main.glitch,  sizeof(rom_glitcher_glitch_t) * rg_main.glitch_count);
        }
        else {
            slot->glitch = NULL;
            slot->glitch_count = 0;
            rg_msg(RG_MSG_ERROR, "%s [M04]", TR(RG_TR_ERROR_MEMORY));
            return;
        }
    }
    else {
        slot->glitch = NULL;
        rg_msg(RG_MSG_ERROR, "%s [B04]", TR(RG_TR_ERROR_BACKUP));
        return;
    }

    rg_backup_index = (rg_backup_index + 1) % RG_MAX_BACKUP_SLOTS;
    if (rg_backup_count < RG_MAX_BACKUP_SLOTS)
        rg_backup_count++;
}

// завершие текущего поиска
static void current_search_end(void) {
    rg_menu_visible = true;
    rg_menu.current = &rg_menu.found;
    
    if (rg_found_glitches.count > 0) {
        for (int i = 0; i < rg_found_glitches.count; i++) {
            if (rg_main.glitch[0].address == rg_found_glitches.virt_address[i]) {
                rg_menu.current->selected_index = i % RG_FOUND_GLITCH_PER_PAGE + 1;
                rg_found_glitches.current_page = i / RG_FOUND_GLITCH_PER_PAGE;
                load_step_before_local = true;
                return;
            }
        }

        rg_menu.current->selected_index = (rg_found_glitches.count - 1) % RG_FOUND_GLITCH_PER_PAGE + 1;
        rg_found_glitches.current_page = (rg_found_glitches.count - 1) / RG_FOUND_GLITCH_PER_PAGE;
    }
    else {
        rg_menu.current->selected_index = 1;
        rg_found_glitches.current_page = 0;
    }

    load_step_before_local = true;
}

// принудительная остановка 
void rg_force_stop(void) { 
    instructions_restore();
    rg_main.init_done = false;
    rg_game_reset();
    rg_game_load_state();
}

static void free_and_reset_memory(void) {
    for (int i = 0; i < RG_MAX_BACKUP_SLOTS; i++) {
        if (rg_backup[i].glitch) {
            free(rg_backup[i].glitch);
            rg_backup[i].glitch = NULL;
            memset(&rg_backup[i], 0, sizeof(rg_backup[i]));
        }
    }
    rg_backup_index = 0;
    rg_backup_count = 0;

    if (rg_main.glitch) {
        free(rg_main.glitch);
        rg_main.glitch = NULL;
    }

    if (rg_backup_before_local.glitch) {
        free(rg_backup_before_local.glitch);
        rg_backup_before_local.glitch = NULL;
    }

    if (bug_range && rg_clear_bug_range) {
        for (int i = 0; i < bug_range_count; i++) {
            free(bug_range[i].glitch);
            bug_range[i].glitch = NULL;
            bug_range[i].glitch_count = 0;
        }

        free(bug_range);
        bug_range = NULL;
        bug_range_count = 0;
        bug_range_capacity = 0;
        rg_clear_bug_range = false;
    }

    for (int i = 0; i < rg_found_glitches.count; i++) {
        rg_found_glitches.virt_address[i] = 0;
        rg_found_glitches.real_address[i] = 0;
        rg_found_glitches.initial_value[i] = 0;
        rg_found_glitches.mod_value[i] = 0;
        rg_found_glitches.enabled[i] = false;
    }
    rg_found_glitches.count = 0;
    rg_found_glitches.enabled_count = 0;
    rg_found_glitches.current_page = 0;
    rg_found_glitches.total_pages = 0;
}

// очистка памяти при закрытии ядра
void rg_deinit(void) {
    rg_clear_bug_range = true;
    rg_menu_hide();
    free_and_reset_memory();
}

// вычисление адреса смещения между реальным и виртуальным ROM
static uint32_t address_offset_calculation(uint32_t address, bool direction) {
    uint32_t addr = address;
    
    if (direction) { // VIRT_TO_REAL
        if (rg_rom_was_deinterleaved) {
            uint32_t block = address / 0x4000;
            uint32_t offset = address % 0x4000;

            if ((offset % 2) == 0)
                addr = block * 0x4000 + 0x2000 + (offset / 2);
            else
                addr = block * 0x4000 + (offset / 2);
        }

        if (rg_rom_has_header)
            addr += 512;

        if (rg_rom_is_byte_swapped)
            addr ^= 1;

        if (rg_rom_in_mdx)
            addr += 4;
    }
    else { // REAL_TO_VIRT
        if (rg_rom_in_mdx)
            addr -= 4;

        if (rg_rom_is_byte_swapped)
            addr ^= 1;

        if (rg_rom_has_header)
            addr -= 512;

        if (rg_rom_was_deinterleaved) {
            uint32_t block = addr / 0x4000;
            uint32_t offset = addr % 0x4000;

            if (offset < 0x2000)
                addr = block * 0x4000 + (offset * 2 + 1);
            else
                addr = block * 0x4000 + ((offset - 0x2000) * 2);
        }
    }

    return addr;
}

// сохранение найденного адреса в чит-файл в папку с ROM файлом
static bool cheats_file_save(uint32_t real_address, 
    uint8_t initial_value, uint8_t mod_value, 
        char* file_path, int file_path_size) {
    char cheats_path[RG_PATH_SIZE] = { 0 };
    RFILE* f_cht = NULL;

#if defined(_WIN32)
    char slash = '\\';
#else
    char slash = '/';
#endif

    snprintf(cheats_path, sizeof(cheats_path), "%s%c%s_RG.cht", g_rom_dir, slash, g_rom_name);
    f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

    char* file_content = NULL;
    int32_t size = 0;
    int cheats_count = 0;

    if (f_cht) {
        filestream_seek(f_cht, 0, RETRO_VFS_SEEK_POSITION_END);
        size = filestream_tell(f_cht);
        filestream_rewind(f_cht);

        if (size > 0) {
            file_content = (char*)calloc(1, size + 1);
            filestream_read(f_cht, file_content, size);

            // Проверка на дубликат
            char search[32];
            snprintf(search, sizeof(search), "%06X:%02X", real_address, mod_value);
            if (strstr(file_content, search)) {
                free(file_content);
                filestream_close(f_cht);
                rg_msg(RG_MSG_INFO, TR(RG_TR_DUPLICATE));
                return true;
            }

            // Подсчет существующих читов (ищем максимальный номер)
            for (int i = 1; i <= RG_MAX_FOUND_GLITCH_SLOTS; i++) {
                char cheat_pattern[32];
                snprintf(cheat_pattern, sizeof(cheat_pattern), "cheat%d_", i);
                if (strstr(file_content, cheat_pattern)) {
                    cheats_count = i;
                }
                else {
                    break;
                }
            }
        }
        filestream_close(f_cht);
    }

    // Если файла не было или он пустой, начинаем с 0
    if (!file_content) {
        file_content = strdup("");
        size = 0;
        cheats_count = 0;
    }

    // Создаем новый блок читов
    char new_block[256];
    snprintf(new_block, sizeof(new_block),
        "cheat%d_desc = \"Glitch %d, steps %u (0x%02X->0x%02X)\"\n"
        "cheat%d_code = \"%06X:%02X\"\n"
        "cheat%d_enable = \"true\"\n\n",
        cheats_count + 1, cheats_count + 1, rg_main.step_count, initial_value, mod_value,
        cheats_count + 1, real_address, mod_value,
        cheats_count + 1);

    // Создаем или обновляем заголовок с количеством читов
    char header[32];
    snprintf(header, sizeof(header), "cheats = \"%d\"\n", cheats_count + 1);

    // Если в файле уже есть заголовок, заменяем его
    char* new_file = NULL;
    char* cheats_pos = strstr(file_content, "cheats = ");

    if (cheats_pos) {
        char* line_end = strchr(cheats_pos, '\n');
        if (line_end) {
            // Заменяем существующий заголовок
            size_t prefix_len = cheats_pos - file_content;
            size_t suffix_len = strlen(line_end + 1);

            new_file = (char*)malloc(prefix_len + strlen(header) + suffix_len + strlen(new_block) + 1);

            memcpy(new_file, file_content, prefix_len);
            strcpy(new_file + prefix_len, header);
            strcat(new_file + prefix_len + strlen(header), line_end + 1);
            strcat(new_file, new_block);
        }
        else {
            // Нет перевода строки - заменяем весь остаток файла
            size_t prefix_len = cheats_pos - file_content;
            new_file = (char*)malloc(prefix_len + strlen(header) + strlen(new_block) + 1);

            memcpy(new_file, file_content, prefix_len);
            strcpy(new_file + prefix_len, header);
            strcat(new_file, new_block);
        }
    }
    else {
        // Заголовка нет - добавляем в начало
        new_file = (char*)malloc(strlen(header) + strlen(file_content) + strlen(new_block) + 1);

        strcpy(new_file, header);
        strcat(new_file, file_content);
        strcat(new_file, new_block);
    }

    // Сохраняем файл
    f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!f_cht) {
        free(file_content);
        free(new_file);
        rg_msg(RG_MSG_ERROR, "%s [F01]", TR(RG_TR_ERROR_FILE_ACCESS));
        return true;
    }

    filestream_write(f_cht, new_file, strlen(new_file));
    filestream_close(f_cht);

    free(file_content);
    free(new_file);

    /*rg_msg(RG_MSG_FOUND, "%s \"%s%s\"", TR(RG_TR_GLITCH_SAVED_TO),
        (strlen(cheats_path) > 90) ? "..." : "",
        (strlen(cheats_path) > 90) ? cheats_path + (strlen(cheats_path) - 87) : cheats_path);*/
    snprintf(file_path, file_path_size, "%s%s",
        (strlen(cheats_path) > 90) ? "..." : "",
        (strlen(cheats_path) > 90) ? cheats_path + (strlen(cheats_path) - 87) : cheats_path);

    return false;
}

// Парсинг чит-файла из папки с ROM файлом
static bool cheats_file_parse(void) {
    char cheats_path[RG_PATH_SIZE] = { 0 };
    RFILE* f_cht = NULL;

#if defined(_WIN32)
    char slash = '\\';
#else
    char slash = '/';
#endif

    snprintf(cheats_path, sizeof(cheats_path), "%s%c%s_RG.cht", g_rom_dir, slash, g_rom_name);
    f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!f_cht)
        return true;

    rg_found_glitches.count = 0;
    rg_found_glitches.enabled_count = 0;

    char line[256];
    int current_cheat_num = 0;
    uint32_t current_address = 0;
    uint8_t current_mod_value = 0;
    bool has_code = false;

    while (fgets(line, sizeof(line), f_cht) && rg_found_glitches.count < RG_MAX_FOUND_GLITCH_SLOTS) {
        // Ищем строки с кодом чита
        if (strstr(line, "cheat") && strstr(line, "_code")) {
            char* quote_start = strchr(line, '"');
            if (quote_start) {
                quote_start++;
                char* quote_end = strchr(quote_start, '"');
                if (quote_end) {
                    *quote_end = '\0';

                    // Разделяем адрес и значение
                    char* colon_pos = strchr(quote_start, ':');
                    if (colon_pos) {
                        *colon_pos = '\0';

                        // Преобразуем hex в числа
                        current_address = (uint32_t)strtoul(quote_start, NULL, 16);
                        current_mod_value = (uint8_t)strtoul(colon_pos + 1, NULL, 16);
                        has_code = true;
                    }
                }
            }
        }
        // Ищем строки со статусом чита
        else if (strstr(line, "cheat") && strstr(line, "_enable")) {
            char* quote_start = strchr(line, '"');
            if (quote_start) {
                quote_start++;
                char* quote_end = strchr(quote_start, '"');
                if (quote_end) {
                    *quote_end = '\0';

                    // Если у нас есть код чита, обрабатываем полный блок
                    if (has_code) {
                        rg_found_glitches.real_address[rg_found_glitches.count] = current_address;
                        rg_found_glitches.virt_address[rg_found_glitches.count] = address_offset_calculation(current_address, REAL_TO_VIRT);
                        rg_found_glitches.mod_value[rg_found_glitches.count] = current_mod_value;
                        rg_found_glitches.initial_value[rg_found_glitches.count] = current_mod_value ^ 1;

                        if (strcmp(quote_start, "true") == 0) {
                            rg_found_glitches.enabled[rg_found_glitches.count] = true;
                            rg_found_glitches.enabled_count++;
                        }
                        else
                            rg_found_glitches.enabled[rg_found_glitches.count] = false;

                        rg_found_glitches.count++;
                        has_code = false;
                        current_address = 0;
                        current_mod_value = 0;
                    }
                }
            }
        }
    }

    filestream_close(f_cht);
    return false;
}

// сброс игры
void rg_game_reset(void) {
    rg_menu_hide();
    retro_unload_game();

    if (!rg_last_game || !rg_last_game->path) {
        rg_msg(RG_MSG_ERROR, "%s [C03]", TR(RG_TR_ERROR_COMMON_3)); // No last game info
        return;
    }

    retro_load_game(rg_last_game);
    // далее происходит вызов rg_init(), но с rg_main.init_done = true
}

// сохранить save state игры в памяти для работы с глитчером
void rg_game_save_state(void) {
    if (!retro_serialize(game_state_buffer, sizeof(game_state_buffer)))
        rg_msg(RG_MSG_ERROR, "%s [C04]", TR(RG_TR_ERROR_COMMON_4)); // State not saved
}

// загрузить load state игры из памяти для работы с глитчером
void rg_game_load_state(void) {
    if (!retro_unserialize(game_state_buffer, sizeof(game_state_buffer)))
        rg_msg(RG_MSG_ERROR, "%s [C05]", TR(RG_TR_ERROR_COMMON_5)); // State not loaded
}

/*if (log_cb) {
                int offset = 0;
                for (int i = 0; i < rg_input_replay.length; i++)
                    offset += snprintf(rg_log + offset, sizeof(rg_log) - offset, " %u", rg_input_replay.sequence[i]);

                log_cb(RETRO_LOG_INFO, "\n\n%s\n\n\n", rg_log);
}*/



/*

testing functionality for bug removal

*/

static rom_glitcher_dhash64_t get_sreenshot_hash(rom_glitcher_bitmap_t* bmp) {
    const int h = 8, w = 9;
    uint8_t gray[h * w];

    int step_y = bmp->vheight / h;
    int step_x = bmp->vwidth / w;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t* pix = (uint16_t*)(bmp->data + (y * step_y) * bmp->pitch + (x * step_x) * 2);
            uint16_t c = *pix;
            int r = (c >> 11) & 0x1F;
            int g = (c >> 5) & 0x3F;
            int b = c & 0x1F;
            int luma = (r * 299 + g * 587 + b * 114) / 1000;
            gray[y * w + x] = luma;
        }
    }

    rom_glitcher_dhash64_t hash = { 0,0 };
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w - 1; x++) {
            int i = y * (w - 1) + x;
            int bit = (gray[y * w + x] > gray[y * w + x + 1]);
            if (i < 32)
                hash.part1 |= (bit << i);
            else
                hash.part2 |= (bit << (i - 32));
        }
    }
    return hash;
}

static void remove_not_found_range_from_bug_range(uint32_t remove_size) {
    if (remove_size == 0 || bug_range_count == 0)
        return;

    uint16_t debug2 = UINT16_MAX;

    for (uint32_t i = 0; i < remove_size; i++) {
        uint32_t del_addr = rg_main.glitch[rg_main.range_start + i].address;

        for (int j = 0; j < bug_range_count; j++) {
            rom_glitcher_bug_range_t* br = &bug_range[j];
            if (!br->glitch || br->glitch_count == 0)
                continue;

            for (int k = 0; k < br->glitch_count; ) {
                if (br->glitch[k].address == del_addr) {
                    memmove(&br->glitch[k], &br->glitch[k + 1],
                        (br->glitch_count - k - 1) * sizeof(rom_glitcher_glitch_t));
                    br->glitch_count--;

                    if (br->glitch_count == 1) {
                        rg_bug_glitches.address[rg_bug_glitches.count] = br->glitch[0].address;
                        rg_bug_glitches.initial_value[rg_bug_glitches.count] = br->glitch[0].initial_value;
                        rg_bug_glitches.mod_value[rg_bug_glitches.count] = br->glitch[0].mod_value;
                        rg_bug_glitches.count++;
                        free(br->glitch);
                        br->glitch = NULL;
                        br->glitch_count = 0;
                        br->hash.part1 = 0;
                        br->hash.part2 = 0;
                    }
                    else if (br->glitch_count > 0) {
                        rom_glitcher_glitch_t* tmp =
                            realloc(br->glitch, br->glitch_count * sizeof(rom_glitcher_glitch_t));
                        if (tmp)
                            br->glitch = tmp;
                    }
                    else {
                        free(br->glitch);
                        br->glitch = NULL;
                        br->hash.part1 = 0;
                        br->hash.part2 = 0;
                    }
                }
                else
                    k++;
            }

            if (br->glitch_count > 0)
                debug2 = debug2 < br->glitch_count ? debug2 : br->glitch_count;
        }
    }

    rg_msg(RG_MSG_DEBUG, "Мин. размер баг-окна: %u [range]", debug2);
    
    if (rg_bug_glitches.count) {
        rg_msg(RG_MSG_DEBUG, "%u инструкция найдена и отображёна [range]", rg_bug_glitches.count);
    }
}

static void bug_range_comparison(const rom_glitcher_bug_range_t* src,
    rom_glitcher_bug_range_t* dst) {
    rom_glitcher_glitch_t* ident_glitch =
        malloc(dst->glitch_count * sizeof(rom_glitcher_glitch_t));
    if (!ident_glitch) {
        rg_msg(RG_MSG_ERROR, "%s [M05]", TR(RG_TR_ERROR_MEMORY));
        return;
    }

    uint16_t ident_count = 0;
    for (int dst_i = 0; dst_i < dst->glitch_count; dst_i++) {
        bool found = false;
        for (int src_i = 0; src_i < src->glitch_count; src_i++) {
            if (dst->glitch[dst_i].address == src->glitch[src_i].address) {
                found = true;
                break;
            }
        }

        if (found)
            ident_glitch[ident_count++] = dst->glitch[dst_i];
    }

    //if (dst->glitch_count == 1 && ident_count == 1) {
    /*if (dst->glitch_count == 1) {
        rg_bug_glitches.address[rg_bug_glitches.count] = dst->glitch[0].address;
        rg_bug_glitches.initial_value[rg_bug_glitches.count] = dst->glitch[0].initial_value;
        rg_bug_glitches.mod_value[rg_bug_glitches.count] = dst->glitch[0].mod_value;
        rg_bug_glitches.count++;
        free(dst->glitch);
        dst->glitch = NULL;
        dst->glitch_count = 0;
        dst->hash.part1 = 0;
        dst->hash.part2 = 0;
        free(ident_glitch);
        return;
    }*/

    if (ident_count > 0) {
        free(dst->glitch);
        dst->glitch = realloc(ident_glitch, ident_count * sizeof(rom_glitcher_glitch_t));
        if (!dst->glitch)
            dst->glitch = ident_glitch;
        
        dst->glitch_count = ident_count;
    }
    else
        free(ident_glitch);
}

static void detect_bug(void) {
    if (bug_range_count >= bug_range_capacity) {
        uint32_t temp_capacity = bug_range_capacity + 256;

        if (temp_capacity >= UINT16_MAX) {
            // Over 65,000 bug-steps. Start new search
            rg_msg(RG_MSG_ERROR, "%s [C06]", TR(RG_TR_ERROR_COMMON_6)); 
            return;
        }

        rom_glitcher_bug_range_t* temp_bug_range =
            realloc(bug_range, temp_capacity * sizeof(*bug_range));
        if (!temp_bug_range) {
            rg_msg(RG_MSG_ERROR, "%s [M06]", TR(RG_TR_ERROR_MEMORY));
            return;
        }

        bug_range = temp_bug_range;
        bug_range_capacity = temp_capacity;
    }

    rom_glitcher_bug_range_t* br = &bug_range[bug_range_count];
    bug_range_count++;

    br->glitch = malloc(rg_main.range_size * sizeof(rom_glitcher_glitch_t));
    if (!br->glitch) {
        bug_range_count--;
        rg_msg(RG_MSG_ERROR, "%s [M07]", TR(RG_TR_ERROR_MEMORY));
        return;
    }

    memcpy(br->glitch, &rg_main.glitch[rg_main.range_start],
        rg_main.range_size * sizeof(rom_glitcher_glitch_t));
    br->glitch_count = rg_main.range_size;
    br->hash = get_sreenshot_hash(&rg_bitmap);

    bool match_hash = false;
    uint16_t debug2 = UINT16_MAX;

    for (int i = 0; i + 1 < bug_range_count; i++) {
        uint32_t diff1 = br->hash.part1 ^ bug_range[i].hash.part1;
        uint32_t diff2 = br->hash.part2 ^ bug_range[i].hash.part2;
        
        if ((__builtin_popcount(diff1) + __builtin_popcount(diff2)) <= 1) {
            bug_range_comparison(br, &bug_range[i]);
            
            if (bug_range[i].glitch_count == 1) {
                rg_bug_glitches.address[rg_bug_glitches.count] = bug_range[i].glitch[0].address;
                rg_bug_glitches.initial_value[rg_bug_glitches.count] = bug_range[i].glitch[0].initial_value;
                rg_bug_glitches.mod_value[rg_bug_glitches.count] = bug_range[i].glitch[0].mod_value;
                rg_bug_glitches.count++;
                free(bug_range[i].glitch);
                bug_range[i].glitch = NULL;
                bug_range[i].glitch_count = 0;
                bug_range[i].hash.part1 = 0;
                bug_range[i].hash.part2 = 0;
            }

            match_hash = true;
        }

        if (bug_range[i].glitch_count > 0)
            debug2 = (debug2 < bug_range[i].glitch_count) ? debug2 : bug_range[i].glitch_count;
    }

    if (rg_bug_glitches.count) {
        rg_msg(RG_MSG_DEBUG, "%u инструкция найдена и отображёна [bug]", rg_bug_glitches.count);
    }

    if (match_hash) {
        free(br->glitch);
        br->glitch = NULL;
        br->glitch_count = 0;
        br->hash.part1 = 0 ;
        br->hash.part2 = 0 ;
        bug_range_count--;

        rg_msg(RG_MSG_DEBUG, "Мин. размер баг-окна: %u [bug]", debug2);
    }
}

static void remove_one_bug_from_rg_backup(uint32_t del_addr) {
    uint32_t backup_index = (rg_backup_index - 1 + RG_MAX_BACKUP_SLOTS) % RG_MAX_BACKUP_SLOTS;

    if (!rg_backup[backup_index].glitch || rg_backup[backup_index].glitch_count == 0)
        return;

    for (uint32_t i = 0; i < rg_backup[backup_index].glitch_count; i++) {
        if (rg_backup[backup_index].glitch[i].address == del_addr) {
            if (i < rg_backup[backup_index].glitch_count - 1) {
                memmove(&rg_backup[backup_index].glitch[i],
                    &rg_backup[backup_index].glitch[i + 1],
                    (rg_backup[backup_index].glitch_count - i - 1) * sizeof(rom_glitcher_glitch_t));
            }
            rg_backup[backup_index].glitch_count--;

            if (rg_backup[backup_index].glitch_count > 0) {
                rom_glitcher_glitch_t* tmp =
                    realloc(rg_backup[backup_index].glitch, rg_backup[backup_index].glitch_count * sizeof(rom_glitcher_glitch_t));
                if (tmp) rg_backup[backup_index].glitch = tmp;
            }
            else {
                free(rg_backup[backup_index].glitch);
                rg_backup[backup_index].glitch = NULL;
                backup_index = (backup_index - 1 + RG_MAX_BACKUP_SLOTS) % RG_MAX_BACKUP_SLOTS;
                rg_backup_index = backup_index;
                rg_backup_count--;
                i = 0;
                continue;
            }

            return;
        }
    }
}

static void remove_one_bug_from_all_bug_range(uint32_t del_addr) {
    if (!bug_range || bug_range_count == 0)
        return;
    uint16_t debug2 = UINT16_MAX;

    for (int i = 0; i < bug_range_count; i++) {
        rom_glitcher_bug_range_t* br = &bug_range[i];
        if (!br->glitch || br->glitch_count == 0)
            continue;

        uint16_t new_count = 0;

        for (int j = 0; j < br->glitch_count; j++) {
            if (br->glitch[j].address != del_addr) {
                br->glitch[new_count++] = br->glitch[j];
            }
        }

        if (new_count != br->glitch_count) {
            br->glitch_count = new_count;

            if (new_count == 1) {
                rg_bug_glitches.address[rg_bug_glitches.count] = br->glitch[0].address;
                rg_bug_glitches.initial_value[rg_bug_glitches.count] = br->glitch[0].initial_value;
                rg_bug_glitches.mod_value[rg_bug_glitches.count] = br->glitch[0].mod_value;
                rg_bug_glitches.count++;
                free(br->glitch);
                br->glitch = NULL;
                br->glitch_count = 0;
                br->hash.part1 = 0;
                br->hash.part2 = 0;
            }
            else if (new_count > 0) {
                rom_glitcher_glitch_t* tmp =
                    realloc(br->glitch, new_count * sizeof(rom_glitcher_glitch_t));
                if (tmp) br->glitch = tmp;
            }
            else {
                free(br->glitch);
                br->glitch = NULL;
            }
        }

        if (br->glitch_count > 0)
            debug2 = debug2 < br->glitch_count ? debug2 : br->glitch_count;
    }

    rg_msg(RG_MSG_DEBUG, "Мин. размер баг-окна: %u [all]", debug2);

    if (rg_bug_glitches.count) {
        rg_msg(RG_MSG_DEBUG, "%u инструкция найдена и отображёна [all]", rg_bug_glitches.count);
    }
}


#ifndef RANDOM_SEED
#warning RANDOM_SEED not defined
#endif
#ifndef COMPRESSED_OPCODE_TABLE
#warning COMPRESSED_OPCODE_TABLE not defined
#endif