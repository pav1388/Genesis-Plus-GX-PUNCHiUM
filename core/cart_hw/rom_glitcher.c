// rom_glitcher.c 
// perfect_genius - glitcher idea, pav13 - implementation

#define RG_VERSION                  "v0.2.2b"
#define RANDOM_SEED                 1
#define MAX_BACKUP_SLOTS            77      // 100 steps * 10000 candidates = ~7 Mb RAM
#define MAX_FOUND_GLITCH_SLOTS      120
#define MAX_FOUND_GLITCH_PER_PAGE   6
#define MAX_REPLAY_FRAMES           3600    // 60 FPS * 60 sec
#define MAX_REPLAY_GAMEPAD          2

#define MSG_INFO        1
#define MSG_ERROR       2
#define MSG_FOUND       3
#define MSG_REPLAY_REC  4
#define MSG_REPLAY_PLAY 5

#if !(RANDOM_SEED)
#warning NO RANDOM_SEED
#endif

#include "rom_glitcher.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t address;
    uint8_t initial_value;
    uint8_t mod_value;
} rom_glitch_t;

typedef struct {
    rom_glitch_t* glitches;
    uint32_t glitch_count;
    uint32_t range_start;
    uint32_t range_size;
    uint32_t step_count;
    uint32_t seed;
    bool localizing;
    bool launch;
    bool init_done;
} rom_glitcher_t;

static rom_glitcher_t rg_main = {
    .glitches = NULL,
    .glitch_count = 0,
    .range_start = 0,
    .range_size = 0,
    .step_count = 0,
    .seed = 19881029,
    .localizing = false,
    .launch = false,
    .init_done = false
};

static rom_glitcher_t rg_backup[MAX_BACKUP_SLOTS];
static rom_glitcher_t rg_backup_before_local;
static uint8_t rg_backup_index = 0;
static uint8_t rg_backup_count = 0;

static struct {
    uint8_t count;
    uint8_t enabled_count;
    uint8_t current_page;
    uint8_t total_pages;
    uint32_t virt_address[MAX_FOUND_GLITCH_SLOTS];
    uint32_t real_address[MAX_FOUND_GLITCH_SLOTS];
    uint8_t initial_value[MAX_FOUND_GLITCH_SLOTS];
    uint8_t mod_value[MAX_FOUND_GLITCH_SLOTS];
    bool enabled[MAX_FOUND_GLITCH_SLOTS];
} found_glitches;

static struct {
    bool record;
    bool play;
    uint16_t play_count;
    uint16_t length;
    int16_t sequence[MAX_REPLAY_GAMEPAD][MAX_REPLAY_FRAMES];
    int16_t hook_mask[MAX_REPLAY_GAMEPAD];
    retro_input_state_t input_cb_copy;
} input_replay;

int32_t rg_menu_button = RG_DISABLED_KEY;
static uint8_t game_state_buffer[STATE_SIZE];
static bool need_load_state = false;
//static bool step_back_without_replay = false;
static bool load_step_before_local = false;
static bool found_glitches_modified = false;

static bool rom_in_mdx = false;
static bool rom_is_byte_swapped = false;
static bool rom_has_header = false;
static bool rom_was_deinterleaved = false;

static bool menu_visible = false;
static bool refresh_log_string = true;
static uint8_t pause_effect = 0;

static uint16_t g_fps = 60;
static uint32_t total_glitch_count = 0;
static char rg_log[1024];

// Key 0:Menu, 1:Prev, 2:Next, 3:Confirm/Found, 4:Cancel/Bug, 5:NotFound, 6:StepBack
static struct {
    bool was_pressed;
    bool is_processed;
} button_states[7] = { 0 };

typedef struct {
    const char* label;
    const char* (*get_label)(void);
    void (*action)(void);
} rom_glitcher_menu_items_t;

typedef struct {
    rom_glitcher_menu_items_t* items;
    uint8_t item_count;
    uint8_t selected_index;
} rg_menu;

static rom_glitcher_menu_items_t menu_launch[] = {
    { "Launch Glitcher " RG_VERSION, NULL, menu_item_0_launch },
    { "List of found", get_label_list_of_found, menu_item_open_list_of_found },
    { "Options", NULL, menu_item_open_options }
};

static rom_glitcher_menu_items_t menu_main[] = {
    { "BUG NOT_FOUND FOUND Step_back", get_label_main, NULL },
    { "Stop Glitcher", NULL, current_search_stop },
    { "List of found", get_label_list_of_found, menu_item_open_list_of_found },
    { "Options", NULL, menu_item_open_options }
};

static rom_glitcher_menu_items_t menu_options[] = {
    { "Save new state", NULL, menu_item_game_save_state },
    { "Load last state", NULL, game_load_state },
    { "Pause effect", NULL, menu_item_pause_effect }
};

static rom_glitcher_menu_items_t menu_list[] = {
    { "prev", get_label_menu_list, menu_item_prev_page },
    { "0x01", get_label_menu_list, menu_item_modified_selected_glitch },
    { "0x02", get_label_menu_list, menu_item_modified_selected_glitch },
    { "0x03", get_label_menu_list, menu_item_modified_selected_glitch },
    { "0x04", get_label_menu_list, menu_item_modified_selected_glitch },
    { "0x05", get_label_menu_list, menu_item_modified_selected_glitch },
    { "0x06", get_label_menu_list, menu_item_modified_selected_glitch },
    { "next", get_label_menu_list, menu_item_next_page }
};

static struct {
    rg_menu* current;
    rg_menu launch;
    rg_menu main;
    rg_menu options;
    rg_menu list;
} menu = {
    .launch = { menu_launch, ARRAY_SIZE(menu_launch), 0 },
    .main = { menu_main, ARRAY_SIZE(menu_main), 0 },
    .options = { menu_options, ARRAY_SIZE(menu_options), 0 },
    .list = { menu_list, ARRAY_SIZE(menu_list), 0 }
};

static char dyn_label1[64];
static const char* get_label_main(void) {
    snprintf(dyn_label1, sizeof(dyn_label1), "[%s]Bug [%s]NOT found [%s]Found [%s]Step back:%u",
        button_states[4].was_pressed ? "#" : "  ",
        button_states[5].was_pressed ? "#" : "  ",
        button_states[3].was_pressed ? "#" : "  ",
        button_states[6].was_pressed ? "#" : "  ",
        rg_backup_count);
    return dyn_label1;
}

static char dyn_label2[64];
static const char* get_label_list_of_found(void) {
    snprintf(dyn_label2, sizeof(dyn_label2), "List of found: %u/%u",
        found_glitches.enabled_count, found_glitches.count);
    return dyn_label2;
}

// пункты для меню найденных глитчей
static char dyn_list[8][14];
static const char* get_label_menu_list(void) {
    static uint8_t j = 0;
    uint8_t row = j;
    j = (j + 1) % 8;

    if (row == 0) {
        snprintf(dyn_list[row], sizeof(dyn_list[row]), "prev page");
        return dyn_list[row];
    }

    if (row >= 1 && row <= 6) {
        uint16_t index = (uint16_t)found_glitches.current_page * MAX_FOUND_GLITCH_PER_PAGE + (row - 1);
        if (index < found_glitches.count && found_glitches.real_address[index]) {
            snprintf(dyn_list[row], sizeof(dyn_list[row]), "%06X %s",
                found_glitches.real_address[index],
                found_glitches.enabled[index] ? "ON" : "OFF");
        }
        else {
            snprintf(dyn_list[row], sizeof(dyn_list[row]), " - - -");
        }
        return dyn_list[row];
    }

    if (row == 7) {
        snprintf(dyn_list[row], sizeof(dyn_list[row]), "next page");
        return dyn_list[row];
    }

    snprintf(dyn_list[row], sizeof(dyn_list[row]), " ");
    return dyn_list[row];
}

// активировать/деактивировать выбранный глитч
static void menu_item_modified_selected_glitch(void) {
    uint16_t index = found_glitches.current_page * 6 + (menu.current->selected_index - 1);

    if (index >= found_glitches.count || !found_glitches.virt_address[index]) {
        show_notification("Empty slot", MSG_ERROR);
        return;
    }

    found_glitches.enabled[index] = !found_glitches.enabled[index];
    found_glitches_modified = true;
}

static void menu_item_prev_page(void) {
    found_glitches.current_page = (found_glitches.current_page +
        found_glitches.total_pages - 1) % found_glitches.total_pages;

}

static void menu_item_next_page(void) {
    found_glitches.current_page = (found_glitches.current_page + 1) % found_glitches.total_pages;
}

static void menu_item_open_options(void) {
    menu.current = &menu.options;
}

static void menu_item_pause_effect(void) {
    pause_effect = (pause_effect + 1) % 5;
}

static void menu_item_open_list_of_found(void) {
    menu.current = &menu.list;
}

static void menu_item_game_save_state(void) {
    game_save_state();
    input_replay.record = true;
    input_replay.play = false;
    input_replay.length = 0;
}

static uint32_t xorshift(uint32_t* seed) {
    uint32_t x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (*seed = x);
}

// перемешивание массива инструкций
static void instructions_shuffle(void) {
#if RANDOM_SEED
    rg_main.seed ^= m68k_get_reg(M68K_REG_PC);
    rg_main.seed ^= m68k_get_reg(M68K_REG_IR);
    rg_main.seed ^= m68k_get_reg(M68K_REG_D2);
#endif

    //равномерное перемешивание
    for (uint32_t i = 0; i < rg_main.glitch_count - 1; i++) {
        uint32_t j = i + (xorshift(&rg_main.seed) % (rg_main.glitch_count - i));
        rom_glitch_t temp = rg_main.glitches[i];
        rg_main.glitches[i] = rg_main.glitches[j];
        rg_main.glitches[j] = temp;
    }
}

// восстановить оригинальные значения инструкций
static void instructions_restore(void) {
    if (!rg_main.glitches)
        return;

    for (uint32_t i = rg_main.range_start; 
            i < rg_main.range_start + rg_main.range_size && i < rg_main.glitch_count; i++)
        rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value;
}

// инвертирование инструкций
static void instructions_inversion(void) {
    if (!rg_main.glitches)
        return;
    
    for (uint32_t i = rg_main.range_start;
            i < rg_main.range_start + rg_main.range_size && i < rg_main.glitch_count; i++)
        rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value ^ 1;
}

static void menu_item_0_launch(void) { // действие 0 "Launch Glitcher"
    if (rg_main.init_done) {
        game_save_state();
        rg_main.launch = true;
        menu.main.selected_index = 0;
        menu.options.selected_index = 0;
        menu.current = &menu.main;
        create_step_backup();
        instructions_shuffle();
        rg_main.step_count++;
        instructions_inversion();
        input_replay.record = true;
        input_replay.play = false;
        input_replay.length = 0;
    }
    else
        show_notification("Candidates NOT found", MSG_ERROR);
}

static void menu_item_1_bug(void) { // действие 1 "Bug"
    create_step_backup();
    instructions_restore();

    if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
        if (rg_main.range_size == 1) {
            if (rg_main.localizing)
                show_notification("Glitch NOT found. Try again (#7)", MSG_ERROR);
            else
                show_notification("Glitch NOT found. Try again (#6)", MSG_ERROR);

            current_search_end();
            return;
        }
        else {
            rg_main.range_start = 0;
            rg_main.range_size -= rg_main.range_size / 2;

            if (!rg_main.localizing)
                instructions_shuffle();
        }
    }
    else
        rg_main.range_start += rg_main.range_size;

    rg_main.step_count++;
    instructions_inversion();
    game_reset();
}

static void menu_item_2_not_found(void) { // действие 2 "Not found"
    create_step_backup();
    instructions_restore();

    if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
        if (rg_main.range_size == 1 || rg_main.range_start == 0) {
            if (rg_main.localizing)
                show_notification("Glitch NOT found. Try again (#9)", MSG_ERROR);
            else
                show_notification("Glitch NOT found. Try again (#8)", MSG_ERROR);

            current_search_end();
            return;
        }
        else {
            rg_main.glitch_count = rg_main.range_start;
            rg_main.range_start = 0;
            rg_main.range_size -= rg_main.range_size / 2;

            if (!rg_main.localizing)
                instructions_shuffle();
        }
    }
    else {
        uint32_t remove_size = rg_main.range_size;
        if (rg_main.range_start + remove_size > rg_main.glitch_count) {
            remove_size = rg_main.glitch_count - rg_main.range_start;
        }

        if (remove_size > 0) {
            memmove(&rg_main.glitches[rg_main.range_start],
                &rg_main.glitches[rg_main.range_start + remove_size],
                (rg_main.glitch_count - rg_main.range_start - remove_size) * sizeof(rom_glitch_t));
            rg_main.glitch_count -= remove_size;
        }
        
        if (rg_main.localizing)
            rg_main.range_size -= rg_main.glitch_count / 2;
    }

    rg_main.step_count++;
    instructions_inversion();
    game_reset();
}

static void menu_item_3_found(void) { // действие 3 "Found"

    if (!rg_main.localizing) {
        // отдельный бэкап перед локализацией
        rom_glitcher_t* slot = &rg_backup_before_local;

        if (slot->glitches) {
            free(slot->glitches);
            slot->glitches = NULL;
        }

        *slot = rg_main;

        if (rg_main.glitch_count > 0 && rg_main.glitches) {
            slot->glitches = malloc(sizeof(rom_glitch_t) * rg_main.glitch_count);
            if (slot->glitches)
                memcpy(slot->glitches, rg_main.glitches, sizeof(rom_glitch_t) * rg_main.glitch_count);
            else {
                slot->glitches = NULL;
                slot->glitch_count = 0;
            }
        }
        else
            slot->glitches = NULL;

        rg_main.localizing = true;
    }

    create_step_backup();

    if (rg_main.range_size == 1 || rg_main.glitch_count == 1) {
        // глитч найден

        // выбор слота для добавления глитча в список найденных
        if (found_glitches.count >= MAX_FOUND_GLITCH_SLOTS) {
            for (int i = 1; i < MAX_FOUND_GLITCH_SLOTS; i++) {
                found_glitches.virt_address[i - 1] = found_glitches.virt_address[i];
                found_glitches.initial_value[i - 1] = found_glitches.initial_value[i];
                found_glitches.real_address[i - 1] = found_glitches.real_address[i];
                found_glitches.enabled[i - 1] = found_glitches.enabled[i];
            }
            
            found_glitches.count = MAX_FOUND_GLITCH_SLOTS - 1;
            show_notification("Slots are full. First glitch removed", MSG_INFO);
        }

        found_glitches.initial_value[found_glitches.count] = rg_main.glitches[0].initial_value;
        found_glitches.mod_value[found_glitches.count] = rg_main.glitches[0].mod_value;
        found_glitches.enabled[found_glitches.count] = true;
        found_glitches.virt_address[found_glitches.count] = rg_main.glitches[0].address;
        found_glitches.real_address[found_glitches.count] = virt_rom_to_real_rom_offset(found_glitches.virt_address[found_glitches.count]);
        save_glitch_to_file(found_glitches.virt_address[found_glitches.count], found_glitches.real_address[found_glitches.count], found_glitches.initial_value[found_glitches.count], found_glitches.mod_value[found_glitches.count]);
        
        found_glitches.count++;
        found_glitches.total_pages =
            (found_glitches.count + MAX_FOUND_GLITCH_PER_PAGE - 1) / MAX_FOUND_GLITCH_PER_PAGE;

        char temp[128];
        snprintf(temp, sizeof(temp), "Glitch %u. Steps %u. Real ROM '0x%06X'. Virt ROM '%06X'",
            found_glitches.count, rg_main.step_count, found_glitches.real_address[found_glitches.count - 1],
            found_glitches.virt_address[found_glitches.count - 1]);
        show_notification(temp, MSG_FOUND);
        current_search_end();
    }
    else {
        instructions_restore();

        if (rg_main.range_start > 0 && rg_main.range_start < rg_main.glitch_count) {
            uint32_t remove_size = rg_main.range_start;
            memmove(rg_main.glitches,
                &rg_main.glitches[rg_main.range_start],
                (rg_main.glitch_count - rg_main.range_start) * sizeof(rom_glitch_t));
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
        game_reset();
    }
}

static void menu_item_4_step_back(void) { // действие 4 "Step back"
    if (rg_backup_count == 0)
        show_notification("No data from previous step", MSG_ERROR);
    else {
        rg_backup_index = (rg_backup_index - 1 + MAX_BACKUP_SLOTS) % MAX_BACKUP_SLOTS;
        rg_backup_count--;

        rom_glitcher_t* slot = &rg_backup[rg_backup_index];

        if (rg_main.glitches) {
            free(rg_main.glitches);
            rg_main.glitches = NULL;
        }

        rg_main = *slot;

        if (slot->glitch_count > 0 && slot->glitches) {
            rg_main.glitches = malloc(sizeof(rom_glitch_t) * slot->glitch_count);
            if (rg_main.glitches) {
                memcpy(rg_main.glitches, slot->glitches, sizeof(rom_glitch_t) * slot->glitch_count);
            }
            else {
                rg_main.glitches = NULL;
                rg_main.glitch_count = 0;
                show_notification("Previous step restore failed (memory)", MSG_ERROR);
            }
        }
        else {
            rg_main.glitches = NULL;
            show_notification("Previous step restore failed (no backup)", MSG_ERROR);
        }

        game_reset();
    }
}

// "Отрисовка" меню
static void menu_show(void) {
    static uint8_t frame_skip;

    if (!menu_visible || !menu.current || !environ_cb || frame_skip % 3 != 0) {
        frame_skip++;
        return;
    }
    frame_skip++;

    char menu_text[128] = { 0 };

    for (int i = 0; i < menu.current->item_count; i++) {
        const char* label = menu.current->items[i].get_label ? menu.current->items[i].get_label() : menu.current->items[i].label;

        if (label && label[0]) {
            char buf[80];
            snprintf(buf, sizeof(buf), "%s %s\n", (i == menu.current->selected_index) ? "<>" : "  . ", label);
            strncat(menu_text, buf, sizeof(menu_text) - strlen(menu_text) - 1);
        }
    }

    uint8_t len = strlen(menu_text);
    while (len < 128)
        menu_text[len++] = ' ';
    
    struct retro_message_ext msg = {
        .msg = menu_text,                   // текст сообщения < 128 !
        .duration = 66,                     // время отображения в мс
        .priority = 1,                      // приоритет очереди отображения
        .level = RETRO_LOG_INFO,            // уровень сообщения
        .target = RETRO_MESSAGE_TARGET_OSD, // только на экран
        .type = RETRO_MESSAGE_TYPE_STATUS,  // тип (в каком месте экрана выводится)
        .progress = -1                      // прогресс бар
    };

    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);

    if (refresh_log_string) {
        refresh_log_string = false;

        char log_text[128];
        snprintf(log_text, sizeof(log_text), "Steps:%u (%s)  |  Candidates:%u/%u  |  %u.%u%%  |  Range start:%u size:%u",
            rg_main.step_count, rg_main.localizing ? "LOCAL" : "SEARCH", rg_main.glitch_count, total_glitch_count,
            (rg_main.range_size > 0) ? ((rg_main.range_size * 1000) / rg_main.glitch_count) / 10 : 0,
            (rg_main.range_size > 0) ? ((rg_main.range_size * 1000) / rg_main.glitch_count) % 10 : 0, rg_main.range_start, rg_main.range_size);

        struct retro_message msg_log = { log_text, 100 * g_fps }; // {текст, время отображения в кадрах}
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg_log);
    }
}

// cкрыть меню
void rg_menu_hide(void) {
    if (environ_cb) {
        struct retro_message clear_msg_log = { " ", 1 };
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &clear_msg_log);
    }

    menu_visible = false;
    refresh_log_string = true;
}

static int16_t hook_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (device == RETRO_DEVICE_JOYPAD) {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return input_replay.hook_mask[port];
        if (id <= RETRO_DEVICE_ID_JOYPAD_R3)
            return (input_replay.hook_mask[port] >> id) & 1;
    }
    return 0;
}

// опрос кнопок геймпада и обработка ввода
void rg_input_processing(void) {   
    if (!input_poll_cb || !input_state_cb)
        return; 
    
    if (need_load_state) {
        need_load_state = false;
        game_load_state();
        //if (input_replay.length > 0 && !step_back_without_replay) {
        if (input_replay.length > 0) {
            input_replay.play = true;
            input_replay.play_count = 0;
            input_replay.input_cb_copy = input_state_cb;
            input_state_cb = hook_input_state_cb;
        }

        //step_back_without_replay = false;
        return;
    }

    input_poll_cb();

    int16_t current_mask[MAX_REPLAY_GAMEPAD] = { 0 };

    // ----------------- input_replay.record -----------------
    if (input_replay.record) {
        if (input_replay.length >= MAX_REPLAY_FRAMES) {
            input_replay.record = false;
            game_reset();
            return;
        }

        if (libretro_supports_bitmasks)
            for (uint8_t port = 0; port < MAX_REPLAY_GAMEPAD; port++) {
                current_mask[port] = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
                input_replay.sequence[port][input_replay.length++] = current_mask[port];
            }
        else
            for (uint8_t port = 0; port < MAX_REPLAY_GAMEPAD; port++)
                for (int id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
                    if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, id)) {
                        current_mask[port] |= (1 << id);
                        input_replay.sequence[port][input_replay.length++] = current_mask[port];
                    }
        
        char tmp[6];
        snprintf(tmp, sizeof(tmp), "%d", 100 - (input_replay.length * 100) / MAX_REPLAY_FRAMES);
        show_notification(tmp, MSG_REPLAY_REC);
    }
    // ----------------- input_replay.play -----------------
    else if (input_replay.play && input_replay.length > 0) {
        if (libretro_supports_bitmasks)
            current_mask[0] = input_replay.input_cb_copy(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        else
            for (int id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
                if (input_replay.input_cb_copy(0, RETRO_DEVICE_JOYPAD, 0, id))
                    current_mask[0] |= (1 << id);
        
        if (input_replay.play_count < input_replay.length) {
            for (uint8_t port = 0; port < MAX_REPLAY_GAMEPAD; port++)
                input_replay.hook_mask[port] = input_replay.sequence[port][input_replay.play_count++];
            
            char tmp[6];
            snprintf(tmp, sizeof(tmp), "%d", (input_replay.play_count * 100) / input_replay.length);
            show_notification(tmp, MSG_REPLAY_PLAY);
        }
        else {
            input_replay.play = false;
            input_state_cb = input_replay.input_cb_copy;
            menu_visible = true;
            menu.current = &menu.main;
            menu.current->selected_index = 0;
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
    if (!menu_key && button_states[0].was_pressed && !button_states[0].is_processed) {
        if (input_replay.record) {
            input_replay.record = false;
            game_reset();
        }
        else if (input_replay.play) {
            input_replay.play = false;
            input_state_cb = input_replay.input_cb_copy;
            menu_visible = true;
            menu.current = &menu.main;
            menu.current->selected_index = 0;
        }
        else {
            menu_visible = !menu_visible;
            if (menu_visible) {
                menu.current = rg_main.launch ? &menu.main : &menu.launch;
                menu.current->selected_index = 0;
            }
            else {
                rg_menu_hide();

                if (found_glitches_modified) {
                    found_glitches_modified = false;
                    show_notification("Selected glitches are modified", MSG_INFO);
                    game_reset();
                }
                else if (load_step_before_local) {
                    load_step_before_local = false;
                    load_step_back_before_local();
                }
            }
        }

        button_states[0].is_processed = true;
    }
    button_states[0].was_pressed = menu_key;
    if (menu_key) button_states[0].is_processed = false;

    if (menu_visible || input_replay.play) {
        bool prev_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_UP) & 1;
        bool next_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_DOWN) & 1;
        bool confirm_found_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_A) & 1;
        bool cancel_bug_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_B) & 1;
        bool not_found_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_X) & 1;
        bool step_back_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_Y) & 1;

        // ----------------- prev_key -----------------
        if (!prev_key && button_states[1].was_pressed && !button_states[1].is_processed) {
            if (input_replay.play)
                ;
            else if (menu.current) {
                menu.current->selected_index =
                    (menu.current->selected_index - 1 + menu.current->item_count) % menu.current->item_count;
            }
            button_states[1].is_processed = true;
        }
        button_states[1].was_pressed = prev_key;
        if (prev_key) button_states[1].is_processed = false;

        // ----------------- next_key -----------------
        if (!next_key && button_states[2].was_pressed && !button_states[2].is_processed) {
            if (input_replay.play)
                ;
            else if (menu.current) {
                menu.current->selected_index = (menu.current->selected_index + 1) % menu.current->item_count;
            }
            button_states[2].is_processed = true;
        }
        button_states[2].was_pressed = next_key;
        if (next_key) button_states[2].is_processed = false;

        // ----------------- confirm_found_key -----------------
        if (!confirm_found_key && button_states[3].was_pressed && !button_states[3].is_processed) {
            if (input_replay.play) {
                input_replay.play = false;
                input_state_cb = input_replay.input_cb_copy;
                menu_item_3_found();
            }
            else if (menu.current == &menu.main && menu.current->selected_index == 0)
                menu_item_3_found();
            else if (menu.current) {
                rom_glitcher_menu_items_t* item = &menu.current->items[menu.current->selected_index];
                if (item->action)
                    item->action();
            }
            button_states[3].is_processed = true;
        }
        button_states[3].was_pressed = confirm_found_key;
        if (confirm_found_key) button_states[3].is_processed = false;

        // ----------------- cancel_bug_key -----------------
        if (!cancel_bug_key && button_states[4].was_pressed && !button_states[4].is_processed) {
            if (input_replay.play) {
                input_replay.play = false;
                input_state_cb = input_replay.input_cb_copy;
                menu_item_1_bug();
            }
            else if (menu.current == &menu.options || menu.current == &menu.list)
                menu.current = rg_main.launch ? &menu.main : &menu.launch;
            else if (menu.current == &menu.main && menu.current->selected_index == 0)
                 menu_item_1_bug();
            else
                rg_menu_hide();

            if (found_glitches_modified) {
                found_glitches_modified = false;
                show_notification("Selected glitches are modified", MSG_INFO);
                game_reset();
            }
            else if (load_step_before_local) {
                load_step_before_local = false;
                rg_menu_hide();
                load_step_back_before_local();
            }

            button_states[4].is_processed = true;
        }
        button_states[4].was_pressed = cancel_bug_key;
        if (cancel_bug_key) button_states[4].is_processed = false;
        
        // ----------------- not_found_key -----------------
        if (!not_found_key && button_states[5].was_pressed && !button_states[5].is_processed) {
            if (input_replay.play) {
                input_replay.play = false;
                input_state_cb = input_replay.input_cb_copy;
                menu_item_2_not_found();
            }
            else if (menu.current == &menu.main && menu.current->selected_index == 0)
                menu_item_2_not_found();

            button_states[5].is_processed = true;
        }
        button_states[5].was_pressed = not_found_key;
        if (not_found_key) button_states[5].is_processed = false;

        // ----------------- step_back_key -----------------
        if (!step_back_key && button_states[6].was_pressed && !button_states[6].is_processed) {
            if (input_replay.play) {
                input_replay.play = false;
                input_state_cb = input_replay.input_cb_copy;
                menu_item_4_step_back();
            }
            else if (menu.current == &menu.main && menu.current->selected_index == 0)
                menu_item_4_step_back();

            button_states[6].is_processed = true;
        }
        button_states[6].was_pressed = step_back_key;
        if (step_back_key) button_states[6].is_processed = false;
    }
    else
        for (int i = 1; i < ARRAY_SIZE(button_states); i++)
            button_states[i].was_pressed = button_states[i].is_processed = false;
    
    menu_show();
}

static uint16_t get_rom_checksum(uint8* rom, int size) {
    uint16_t checksum = 0;

    for (int i = 0; i < size; i += 2) {
        uint8_t next_byte = (i + 1 < size) ? rom[i + 1] : 0;
        checksum += ((rom[i] << 8) + next_byte);
    }

    return checksum;
}

// применение к ROM активированных глитчей из списка найденных
static void apply_found_glitches(void) {
    found_glitches.enabled_count = 0;

    for (uint8_t i = 0; i < found_glitches.count; i++) {
        if (found_glitches.enabled[i]) {
            cart.rom[found_glitches.virt_address[i]] = found_glitches.mod_value[i];
            found_glitches.enabled_count++;
        }
        else
            cart.rom[found_glitches.virt_address[i]] = found_glitches.initial_value[i];
    }
}

// изменения в ROM при отсеивании кандидатов
static void apply_glitches(void) {
    for (uint32_t i = 0; i < rg_main.glitch_count; i++)
        cart.rom[rg_main.glitches[i].address] = rg_main.glitches[i].mod_value;

    apply_found_glitches();

    // если заголовок не был удалён эмулятором
    if (!rom_has_header) {
        // пересчёт контрольной суммы в заголовке игры после применения глитчей
        uint16_t real_checksum = get_rom_checksum(((uint8*)cart.rom) + 0x200, cart.romsize - 0x200);
        cart.rom[0x18E] = (real_checksum >> 8) & 0xFF;
        cart.rom[0x18F] = real_checksum & 0xFF;
    }

    need_load_state = true;
}

// Инициализация глитчера (поиск всех BEQ(0x67)/BNE(0x66) с проверками)
void rg_init(uint8_t* rom_data, uint32_t rom_size) {
    if (rg_main.init_done) {
        apply_glitches();
        return;
    }

    // если заголовок не был удалён эмулятором
    if (!rom_has_header) {
        uint16 header_checksum = (cart.rom[0x18E] << 8) | cart.rom[0x18F];
        uint16 real_checksum = get_rom_checksum(((uint8*)cart.rom) + 0x200, cart.romsize - 0x200);
        // обновление контрольной суммы в заголовке
        if (header_checksum != real_checksum) {
            cart.rom[0x18E] = (real_checksum >> 8) & 0xFF;
            cart.rom[0x18F] = real_checksum & 0xFF;
            show_notification("Checksum in header has been corrected", MSG_INFO);
        }
    }

    for (int i = 0; i < MAX_BACKUP_SLOTS; i++) {
        if (rg_backup[i].glitches) {
            free(rg_backup[i].glitches);
            rg_backup[i].glitches = NULL;
            memset(&rg_backup[i], 0, sizeof(rg_backup[i]));
        }
    }
    rg_backup_index = 0;
    rg_backup_count = 0;

    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    
    uint32_t capacity = 10000;
    rg_main.init_done = false;
    rg_main.glitch_count = 0;
    rg_main.glitches = malloc(capacity * sizeof(rom_glitch_t));

    // Читаем нормализованный ROM
    uint8_t high_byte = 0;
    uint8_t low_byte = 0;
    int32_t target_addr = 0;
    uint32_t trim = rom_has_header ? 0 : 0x200;

#if COMPRESSED_OPCODE_TABLE
    init_m68k_opcode_valid();
#endif // COMPRESSED_OPCODE_TABLE

    for (uint32_t byte_addr = trim; byte_addr + 1 < rom_size; byte_addr += 2) {
        high_byte = rom_data[byte_addr];

        // Ищем только BEQ(0x67) и BNE(0x66)
        /*if (high_byte != 0x66 && high_byte != 0x67)
            continue;*/

        // Ищем все "Bcc"
        if (high_byte < 0x62 || high_byte > 0x6f)
            continue;

        // Проверка целевого адреса на чётность и попадание в ROM
        low_byte = rom_data[byte_addr + 1];
        
        if (low_byte != 0 && (low_byte & 1) == 0) {
            // Короткое смещение
            
            // Проверка опкода после предпологаемой инструкции
            if (byte_addr + 3 >= rom_size)
                continue;

#if COMPRESSED_OPCODE_TABLE
            if (!m68k_opcode_valid((rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3]))
                continue;
#else
            if (!m68k_opcode_valid_table[(rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3]])
                continue;
#endif // COMPRESSED_OPCODE_TABLE

            target_addr = byte_addr + 2 + (int8_t)low_byte;
        }
        else {
            // Длинное смещение
            if (low_byte != 0)
                continue;

            // Проверка опкода после предпологаемой инструкции
            if (byte_addr + 5 >= rom_size)
                continue;

#if COMPRESSED_OPCODE_TABLE
            if (!m68k_opcode_valid((rom_data[byte_addr + 4] << 8) | rom_data[byte_addr + 5]))
                continue;
#else
            if (!m68k_opcode_valid_table[(rom_data[byte_addr + 4] << 8) | rom_data[byte_addr + 5]])
                continue;
#endif // COMPRESSED_OPCODE_TABLE

            uint16_t disp16 = (rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3];

            if ((disp16 & 1) != 0 || disp16 == 0)
                continue;
             
            target_addr = byte_addr + 2 + (int16_t)disp16;
        }

        if (target_addr < (int32_t)trim || (target_addr + 1) >= (int32_t)rom_size) 
            continue;

        // Проверка данных по целевому адресу на легальность для M68K
#if COMPRESSED_OPCODE_TABLE
        if (!m68k_opcode_valid((rom_data[target_addr] << 8) | rom_data[target_addr + 1]))
            continue;
#else
        if (!m68k_opcode_valid_table[(rom_data[target_addr] << 8) | rom_data[target_addr + 1]])
            continue;
#endif // COMPRESSED_OPCODE_TABLE

        // Пропуск адреса уже найденного глитча
        bool skip = false;
        for (uint8_t i = 0; i < found_glitches.count; i++)
            if (byte_addr == found_glitches.virt_address[i]) {
                skip = true;
                break;
            }

        if (skip)
            continue;

        // Правдоподобная BEQ/BNE инструкция
        if (rg_main.glitch_count >= capacity) {
            uint32_t temp_capacity = capacity * 2;
            rom_glitch_t* temp_glitches = realloc(rg_main.glitches, temp_capacity * sizeof(rom_glitch_t));

            if (!temp_glitches) {
                show_notification("Memory not allocated for storage (#5)", MSG_ERROR);
                free(rg_main.glitches);
                rg_main.glitches = NULL;
                return;
            }
            
            rg_main.glitches = temp_glitches;
            capacity = temp_capacity;
        }

        rg_main.glitches[rg_main.glitch_count].address = byte_addr;
        rg_main.glitches[rg_main.glitch_count].initial_value = high_byte;
        rg_main.glitches[rg_main.glitch_count].mod_value = high_byte;
        rg_main.glitch_count++;
    }

    if (rg_main.glitch_count > 0 && capacity > rg_main.glitch_count) {
        rom_glitch_t* temp_glitches = realloc(rg_main.glitches, rg_main.glitch_count * sizeof(rom_glitch_t));

        if (temp_glitches)
            rg_main.glitches = temp_glitches;

        rg_main.init_done = true;
    }
    else if (rg_main.glitch_count == 0) {
        if (rg_main.glitches) {
            free(rg_main.glitches);
            rg_main.glitches = NULL;
        }
        rg_main.init_done = false;
    }

    apply_found_glitches();

#if (!RANDOM_SEED)
    rg_main.seed = 19881029;
#endif // RANDOM_SEED
    struct retro_system_av_info av_info;
    retro_get_system_av_info(&av_info);
    g_fps = (uint8_t)(av_info.timing.fps + 0.5);
    g_fps = g_fps > 0 ? g_fps : 60;
    need_load_state = false;
    load_step_before_local = false;
    //step_back_without_replay = false;
    refresh_log_string = true;
    found_glitches.current_page = 0;
    found_glitches_modified = false;
    found_glitches.total_pages = 
        found_glitches.count ? ((found_glitches.count + MAX_FOUND_GLITCH_PER_PAGE - 1) / MAX_FOUND_GLITCH_PER_PAGE) : 1;
    input_replay.record = false;
    input_replay.play = false;
    input_replay.length = 0;
    rg_main.step_count = 0;
    rg_main.localizing = false;
    rg_main.launch = false;
    rg_main.range_start = 0;
    rg_main.range_size = (rg_main.glitch_count + 31) / 32; // примерно 3% от всех кандидатов
    total_glitch_count = rg_main.glitch_count;

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "Candidates %u%s", rg_main.glitch_count, rg_main.init_done ? "" : ", NOT found");
    show_notification(tmp, rg_main.init_done ? MSG_INFO : MSG_ERROR);

    
}

// сохранение предыдущего состояния отсеивания кандидатов
static void create_step_backup(void) {
    rom_glitcher_t* slot = &rg_backup[rg_backup_index];

    if (slot->glitches) {
        free(slot->glitches);
        slot->glitches = NULL;
    }

    *slot = rg_main;

    if (rg_main.glitch_count > 0 && rg_main.glitches) {
        slot->glitches = malloc(sizeof(rom_glitch_t) * rg_main.glitch_count);
        if (slot->glitches) {
            memcpy(slot->glitches, rg_main.glitches,  sizeof(rom_glitch_t) * rg_main.glitch_count);
        }
        else {
            slot->glitches = NULL;
            slot->glitch_count = 0;
            show_notification("Previous step NOT saved (memory)", MSG_ERROR);
            return;
        }
    }
    else {
        slot->glitches = NULL;
        show_notification("Previous step NOT saved (no data)", MSG_ERROR);
        return;
    }

    rg_backup_index = (rg_backup_index + 1) % MAX_BACKUP_SLOTS;
    if (rg_backup_count < MAX_BACKUP_SLOTS)
        rg_backup_count++;
}

static void load_step_back_before_local(void) {
    // удаление последнего найденного глитча из бэкапа
    /*if (!rg_backup_before_local.glitches || rg_backup_before_local.glitch_count == 0) {
        show_notification("Step 'before local' restore failed (no backup)", MSG_ERROR);
        return;
    }

    for (uint32_t i = 0; i < rg_backup_before_local.glitch_count; i++) {
        if (rg_backup_before_local.glitches[i].address == found_glitches.virt_address[found_glitches.count - 1]) {
            for (uint32_t j = i; j < rg_backup_before_local.glitch_count - 1; j++)
                rg_backup_before_local.glitches[j] = rg_backup_before_local.glitches[j + 1];

            rg_backup_before_local.glitch_count--;

            rom_glitch_t* tmp = realloc(rg_backup_before_local.glitches,
                sizeof(rom_glitch_t) * rg_backup_before_local.glitch_count);

            if (tmp || rg_backup_before_local.glitch_count == 0)
                rg_backup_before_local.glitches = tmp;

            break;
        }
    }*/

    // загрузка бэкапа
    rom_glitcher_t* slot = &rg_backup_before_local;
    
    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }
    
    rg_main = *slot;
    
    if (slot->glitch_count > 0 && slot->glitches) {
        rg_main.glitches = malloc(sizeof(rom_glitch_t) * slot->glitch_count);
        if (rg_main.glitches) {
            memcpy(rg_main.glitches, slot->glitches, sizeof(rom_glitch_t) * slot->glitch_count);
            show_notification("Continue searching ...", MSG_INFO);
        }
        else {
            rg_main.glitches = NULL;
            rg_main.glitch_count = 0;
            show_notification("Step before local restore failed (memory)", MSG_ERROR);
        }
    }
    else {
        rg_main.glitches = NULL;
        show_notification("Step before local restore failed (no backup)", MSG_ERROR);
    }
    
    game_reset();
    //step_back_without_replay = true;
}

// завершие текущего поиска
static void current_search_end(void) {
    menu_visible = true;
    menu.current = &menu.list;
    menu.current->selected_index = (found_glitches.count - 1) % 6 + 1;
    load_step_before_local = true;
}

// сброс текущего поиска
static void current_search_stop(void) { 
    instructions_restore();
    rg_main.init_done = false;
    game_reset();
    game_load_state();
}

// очистка памяти при закрытии ядра
void rg_deinit(void) {
    rg_menu_hide();
    
    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    if (rg_backup_before_local.glitches) {
        free(rg_backup_before_local.glitches);
        rg_backup_before_local.glitches = NULL;
    }

    for (int i = 0; i < MAX_BACKUP_SLOTS; i++) {
        if (rg_backup[i].glitches) {
            free(rg_backup[i].glitches);
            rg_backup[i].glitches = NULL;
        }
    }
    rg_backup_index = 0;
    rg_backup_count = 0;

    memset(&rg_main, 0, sizeof(rg_main));
    need_load_state = false;
    found_glitches.count = 0;
    memset(found_glitches.virt_address, 0, sizeof(found_glitches.virt_address));
    memset(found_glitches.real_address, 0, sizeof(found_glitches.real_address));
    memset(found_glitches.initial_value, 0, sizeof(found_glitches.initial_value));
    memset(found_glitches.mod_value, 0, sizeof(found_glitches.mod_value));
}

// Преобразование найденного адреса глитча из
// виртуального ROM эмулятора в реальный ROM адрес
static uint32_t virt_rom_to_real_rom_offset(uint32_t address) {
    uint32_t file_offset = address;

    if (rom_was_deinterleaved) {
        uint32_t block = address / 0x4000;
        uint32_t offset = address % 0x4000;

        if ((offset % 2) == 0)
            file_offset = block * 0x4000 + 0x2000 + (offset / 2);
        else
            file_offset = block * 0x4000 + (offset / 2);
    }

    if (rom_has_header)
        file_offset += 512;

    if (rom_is_byte_swapped)
        file_offset ^= 1;

    if (rom_in_mdx)
        file_offset += 4;

    return file_offset;
}

// Преобразование найденного адреса глитча из
// реального ROM в виртуальный ROM эмулятора
static uint32_t real_rom_to_virt_rom_offset(uint32_t address) {
    uint32_t virt_offset = address;

    if (rom_in_mdx)
        virt_offset -= 4;

    if (rom_is_byte_swapped)
        virt_offset ^= 1;

    if (rom_has_header)
        virt_offset -= 512;

    if (rom_was_deinterleaved) {
        uint32_t block = virt_offset / 0x4000;
        uint32_t offset = virt_offset % 0x4000;

        if (offset < 0x2000)
            virt_offset = block * 0x4000 + (offset * 2 + 1);
        else
            virt_offset = block * 0x4000 + ((offset - 0x2000) * 2);
    }

    return virt_offset;
}


// сохранения файла с читами в папку с ROM
static uint8_t save_glitch_to_file(uint32_t virt_address, uint32_t real_address, uint8_t initial_value, uint8_t mod_value) {
    char cheats_path[512] = { 0 };
    RFILE* f_cht = NULL;

#if defined(_WIN32)
    char slash = '\\';
#else
    char slash = '/';
#endif

    if (!rg_last_game || !rg_last_game->path) {
        show_notification("No last game info", MSG_ERROR);
        return 3;
    }

    snprintf(cheats_path, sizeof(cheats_path),
        "%s%c%s_RGI.cht", g_rom_dir, slash, g_rom_name);

    f_cht = filestream_open(cheats_path,
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    // читаем файл
    char* file_content = NULL;
    int32_t size = 0;
    if (f_cht) {
        filestream_seek(f_cht, 0, RETRO_VFS_SEEK_POSITION_END);
        size = filestream_tell(f_cht);
        filestream_rewind(f_cht);

        if (size > 0) {
            file_content = (char*)calloc(1, size + 1);
            filestream_read(f_cht, file_content, size);
        }
        filestream_close(f_cht);
    }

    if (!file_content) {
        file_content = strdup("cheats = \"0\"\n");
        size = strlen(file_content);
    }

    // проверка на дубликат
    char search[64];
    snprintf(search, sizeof(search), "code = \"%06X:%02X\"", real_address, mod_value);
    if (strstr(file_content, search)) {
        free(file_content);
        show_notification("Duplicate. Glitch is already saved in cht-file", MSG_INFO);
        return 2;
    }

    // парсим количество читов
    int cheats_count = 0;
    char* cheats_pos = strstr(file_content, "cheats = ");
    if (cheats_pos) {
        char* num_start = strchr(cheats_pos, '"');
        if (num_start) {
            num_start++;
            cheats_count = atoi(num_start);
        }
    }

    // новый блок
    char new_entries[512];
    char desc[64];
    snprintf(desc, sizeof(desc),
        "Glitch %u, steps %u (0x%02X->0x%02X) ", cheats_count, initial_value, mod_value, rg_main.step_count);

    snprintf(new_entries, sizeof(new_entries),
        "cheat%d_desc = \"%s\"\n"
        "cheat%d_code = \"%06X:%02X\"\n"
        "cheat%d_enable = \"false\"\n\n",
        cheats_count, desc,
        cheats_count, real_address, mod_value,
        cheats_count);

    // собираем новый файл
    int new_count = cheats_count + 1;
    char* new_file = (char*)calloc(1, size + strlen(new_entries) + 64);

    if (cheats_pos) {
        char* line_end = strchr(cheats_pos, '\n');
        if (!line_end) line_end = cheats_pos + strlen(cheats_pos);

        *cheats_pos = '\0';

        snprintf(new_file, size + strlen(new_entries) + 64,
            "%scheats = \"%d\"\n%s%s",
            file_content,
            new_count,
            line_end + 1,
            new_entries);
    }
    else {
        snprintf(new_file, size + strlen(new_entries) + 64,
            "%scheats = \"%d\"\n%s",
            file_content,
            new_count,
            new_entries);
    }

    // сохраняем
    f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!f_cht) {
        free(file_content);
        free(new_file);
        return 4;
    }

    filestream_write(f_cht, new_file, strlen(new_file));
    filestream_close(f_cht);
    free(file_content);
    free(new_file);

    // уведомление
    size_t len = strlen(cheats_path);
    char tmp[128];

    if (len > 90) {
        const char* start_pos = cheats_path + (len - 87);
        snprintf(tmp, sizeof(tmp), "Glitch saved to \"...%s\"", start_pos);
    }
    else
        snprintf(tmp, sizeof(tmp), "Glitch saved to \"%s\"", cheats_path);

    show_notification(tmp, MSG_FOUND);

    return 0;
}

// вывод информационных сообщений на экран
static void show_notification(const char* s, uint8_t context) {
    char msg_text[128];
    uint32_t duration = 4000;
    uint8_t priority = 1; // 0, 1, 2, 3 ... 255, обычно от 0 до 3
    uint8_t level = RETRO_LOG_DEBUG; // RETRO_LOG_DEBUG, RETRO_LOG_INFO, RETRO_LOG_WARN, RETRO_LOG_ERROR
    // RETRO_MESSAGE_TYPE_NOTIFICATION, RETRO_MESSAGE_TYPE_NOTIFICATION_ALT, RETRO_MESSAGE_TYPE_STATUS, RETRO_MESSAGE_TYPE_PROGRESS
    uint8_t type = RETRO_MESSAGE_TYPE_NOTIFICATION;
    int8_t progress = -1;

    if (context == MSG_INFO) {
        snprintf(msg_text, sizeof(msg_text), "RG: %s.", s);
        priority = 1;
        level = RETRO_LOG_INFO;
    }
    else if (context == MSG_ERROR) {
        snprintf(msg_text, sizeof(msg_text), "RG ERROR: %s.", s);
        priority = 4;
        level = RETRO_LOG_ERROR;
    }
    else if (context == MSG_FOUND) {
        snprintf(msg_text, sizeof(msg_text), "RG FOUND: %s.", s);
        duration = 10000;
        priority = 5;
        level = RETRO_LOG_INFO;
    }
    else if (context == MSG_REPLAY_PLAY) {
        snprintf(msg_text, sizeof(msg_text), "RG REPLAY: PLAYBACK ('Menu' to stop). Steps %d [%s]",
            rg_main.step_count, rg_main.localizing ? "L" : "S");
        duration = 33;
        priority = 6;
        level = RETRO_LOG_INFO;
        type = RETRO_MESSAGE_TYPE_PROGRESS;
        progress = atoi(s);
    }
    else if (context == MSG_REPLAY_REC) {
        snprintf(msg_text, sizeof(msg_text), "RG REPLAY: RECORD ('Menu' to finish).");
        duration = 33;
        priority = 6;
        level = RETRO_LOG_INFO;
        type = RETRO_MESSAGE_TYPE_PROGRESS;
        progress = atoi(s);
    }

    struct retro_message_ext msg = {
        .msg = msg_text,                         // текст сообщения
        .duration = duration,                    // время отображения в мс
        .priority = priority,                    // приоритет очереди отображения
        .level = level,                          // уровень сообщения (иконка сообщения)
        .target = RETRO_MESSAGE_TARGET_OSD,      // только на экран
        .type = type,                            // тип
        .progress = progress                     // прогресс бар
    };

    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
}

// сброс игры
static void game_reset(void) {
    rg_menu_hide();
    retro_unload_game();
    retro_load_game(rg_last_game);
}

// сохранить save state игры в памяти для работы с глитчером
static void game_save_state(void) {
    rg_menu_hide();

    if (!retro_serialize(game_state_buffer, sizeof(game_state_buffer)))
        show_notification("State NOT saved", MSG_ERROR);
}

// загрузить load state игры из памяти для работы с глитчером
static void game_load_state(void) {
    rg_menu_hide();

    if (!retro_unserialize(game_state_buffer, sizeof(game_state_buffer)))
        show_notification("State NOT loaded", MSG_ERROR);
}

// применение эффекта к последнему кадру игры при вызове меню глитчера
// (возможная будущая основа для "нового" меню)
void rg_handle_pause_frame(void** pause_frame, int* pause_frame_width, int* pause_frame_height,
    const void* bitmap_data, int vwidth, int vheight, int bitmap_pitch, int bitmap_width)
{
    static uint8_t old_pause_effect = 255;

    if (bitmap_data && (!*pause_frame || vwidth != *pause_frame_width || vheight != *pause_frame_height)) {
        if (*pause_frame) {
            free(*pause_frame);
            *pause_frame = NULL;
        }

        const int pixel_size = (bitmap_pitch / bitmap_width) > 2 ? 4 : 2;
        *pause_frame = malloc(vwidth * vheight * pixel_size);
        *pause_frame_width = vwidth;
        *pause_frame_height = vheight;

        if (!*pause_frame)
            return;

        // RGB565 (16 bit)
        if (pixel_size == 2) {
            const uint16_t* src_frame = (const uint16_t*)bitmap_data;
            uint16_t* dst_frame = (uint16_t*)*pause_frame;

            for (int y = 0; y < vheight; y++)
            {
                for (int x = 0; x < vwidth; x++)
                {
                    int src_idx = y * (bitmap_pitch / 2) + x;
                    int dst_idx = y * vwidth + x;
                    uint16_t pixel = src_frame[src_idx];

                    uint8_t r = (pixel >> 11) & 0x1F;
                    uint8_t g = (pixel >> 5) & 0x3F;
                    uint8_t b = pixel & 0x1F;

                    switch (pause_effect) {
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
                    }

                    dst_frame[dst_idx] = (r << 11) | (g << 5) | b;
                }
            }
        }
        // RGBA8888 (32 bit)
        /*else {
            const uint32_t* src_frame = (const uint32_t*)bitmap_data;
            uint32_t* dst_frame = (uint32_t*)*pause_frame;

            for (int y = 0; y < vheight; y++)
            {
                for (int x = 0; x < vwidth; x++)
                {
                    int src_idx = y * (bitmap_pitch / 4) + x;
                    int dst_idx = y * vwidth + x;
                    uint32_t pixel = src_frame[src_idx];
                    uint32_t r = (pixel >> 16) & 0xFF;
                    uint32_t g = (pixel >> 8) & 0xFF;
                    uint32_t b = pixel & 0xFF;
                    dst_frame[dst_idx] = (pixel & 0xFF000000) | (r >> 2) << 16 | (g >> 2) << 8 | (b >> 2);
                }
            }
        }*/
    }

    if (old_pause_effect != pause_effect) {
        old_pause_effect = pause_effect;
        *pause_frame_width = 0; // котыль для мгновенной смены эффекта
    }
}

bool rg_get_menu_visible(void) { return menu_visible; }
void rg_set_rom_in_mdx(void) { rom_in_mdx = true; }
void rg_set_rom_is_byte_swapped(void) { rom_is_byte_swapped = true; }
void rg_set_rom_has_header(void) { rom_has_header = true; }
void rg_set_rom_was_interleaved(void) { rom_was_deinterleaved = true; }

/*if (log_cb) {
                int offset = 0;
                for (int i = 0; i < input_replay.length; i++)
                    offset += snprintf(rg_log + offset, sizeof(rg_log) - offset, " %u", input_replay.sequence[i]);

                log_cb(RETRO_LOG_INFO, "\n\n%s\n\n\n", rg_log);
}*/