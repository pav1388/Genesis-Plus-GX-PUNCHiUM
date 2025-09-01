// rom_glitcher.c 
// perfect_genius - glitcher idea, pav13 - implementation

#define RG_VERSION "Launch Glitcher v0.1.2"
#define RG_LOAD_STATE 0
#define RG_HARD_RESET 1
#define RG_MSG_INFO 1
#define RG_MSG_ERROR 2
#define RG_MSG_FOUND 3
#define RG_BACKUP_SLOTS_MAX 9
#define RG_GLITCH_SLOTS_MAX 7 // <= 7 !

#include "rom_glitcher.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    uint32_t address;
    uint8_t initial_value;
    uint8_t mod_value;
} rom_glitch_t;

typedef struct {
    rom_glitch_t* glitches;
    uint32_t glitch_count;
    uint32_t capacity;
    uint32_t range_start;
    uint32_t range_size;
    uint32_t total_glitch_count;
    uint32_t step_count;
    uint32_t bug_step_count;
    bool localizing;
    bool launch;
    bool init_done;
} rom_glitcher_t;

static rom_glitcher_t rg_main = {   // основная структура для работы с глитчами
    .glitches = NULL,
    .glitch_count = 0,
    .capacity = 0,
    .range_start = 0,
    .range_size = 0,
    .total_glitch_count = 0,
    .step_count = 0,
    .bug_step_count = 0,
    .localizing = false,
    .launch = false,
    .init_done = false
};

typedef struct {
    rom_glitcher_t backup;
    rom_glitch_t* glitches_backup;
} rom_glitcher_backup_t;

static rom_glitcher_backup_t rg_backups[RG_BACKUP_SLOTS_MAX];
static int rg_backup_index = 0;
static int rg_backup_count = 0;

typedef struct {
    uint8_t count;
    uint32_t virt_address[RG_GLITCH_SLOTS_MAX];
    uint32_t real_address[RG_GLITCH_SLOTS_MAX];
    uint8_t initial_value[RG_GLITCH_SLOTS_MAX];
    uint8_t mod_value[RG_GLITCH_SLOTS_MAX];
    bool activate[RG_GLITCH_SLOTS_MAX];
} found_glitch_list_t;

static found_glitch_list_t found_glitches = { 0 };

uint8_t rg_menu_button = RG_DISABLED_KEY;
rom_glitcher_callbacks_t rg_cbs;

static char log_text[512];
static uint8_t game_state_buffer[STATE_SIZE];
static bool need_load_state = false;

static bool rom_in_mdx = false;             // rom был в формате MDX?
static bool rom_is_byte_swapped = false;    // ROM был изначально байт-свапнут?
static bool rom_has_header = false;         // был удалён заголовок?
static bool rom_was_deinterleaved = false;  // к ROM применялся деинтерлив?

static bool menu_visible = false;           // отображение меню на экране 
static uint8_t pause_effect = 0;            // эффект игры при паузе 

typedef struct {
    const char* label; // статическая метка
    const char* (*get_label)(void); // функция для создания динамической метки
    void (*action)(void); // действие при активации пункта меню
} rom_glitcher_menu_items_t;

typedef struct {
    rom_glitcher_menu_items_t* items;
    uint8_t item_count;
    uint8_t selected_index;
} rg_menu;

static rom_glitcher_menu_items_t menu_launch[] = {
    { RG_VERSION, NULL, menu_item_0_launch_glitcher },
    { "List of found glitches", get_label_list_of_found_glitches, menu_item_open_list_of_found_glitches },
    { "Options", NULL, menu_item_open_options }
};

static rom_glitcher_menu_items_t menu_main[] = {
    { "BUG NOT_FOUND FOUND Step_back", get_label_main, NULL },
    { "Reset current search", NULL, rg_reset },
    { "Options", NULL, menu_item_open_options }
};

static rom_glitcher_menu_items_t menu_settings[] = {
    { "List of found glitches", get_label_list_of_found_glitches, menu_item_open_list_of_found_glitches },
    { "Load state", NULL, game_load_state_from_ram },
    { "Save state", NULL, game_save_state_to_ram },
    { "Reset game", NULL, game_reset },
    { "Pause effect (test)", NULL, menu_item_pause_effect }
};

static rom_glitcher_menu_items_t menu_list[] = {
    { "0x0", get_label_menu_list, menu_item_activate_selected_glitch },
    { "0x1", get_label_menu_list, menu_item_activate_selected_glitch },
    { "0x2", get_label_menu_list, menu_item_activate_selected_glitch },
    { "0x3", get_label_menu_list, menu_item_activate_selected_glitch },
    { "0x4", get_label_menu_list, menu_item_activate_selected_glitch },
    { "0x5", get_label_menu_list, menu_item_activate_selected_glitch },
    { "0x6", get_label_menu_list, menu_item_activate_selected_glitch }
};

static struct {
    rg_menu* current;
    rg_menu launch;
    rg_menu main;
    rg_menu settings;
    rg_menu list;
} menu = {
    .launch = { menu_launch, ARRAY_SIZE(menu_launch), 0 },
    .main = { menu_main, ARRAY_SIZE(menu_main), 0 },
    .settings = { menu_settings, ARRAY_SIZE(menu_settings), 0 },
    .list = { menu_list, ARRAY_SIZE(menu_list), 0 }
};

static char dyn_list[RG_GLITCH_SLOTS_MAX][14];
static const char* get_label_menu_list(void) { // пункты для меню найденных глитчей
    static uint8_t i = 0;
    uint8_t j = i;
    i = (i + 1) % RG_GLITCH_SLOTS_MAX;

    if (found_glitches.real_address[j]) {
        snprintf(dyn_list[j], sizeof(dyn_list[j]),
            "0x%06X >%u",
            found_glitches.real_address[j],
            found_glitches.activate[j]);
    }
    else
        snprintf(dyn_list[j], sizeof(dyn_list[j]), "empty slot");

    return dyn_list[j];
}

static char dyn_label1[64];
static const char* get_label_main(void) {
    snprintf(dyn_label1, sizeof(dyn_label1), "[B]BUG  [X]NOT_FOUND  [A]FOUND  [Y]Step_back:%u", rg_backup_count);
    return dyn_label1;
}

static char dyn_label2[64];
static const char* get_label_list_of_found_glitches(void) {
    snprintf(dyn_label2, sizeof(dyn_label2), "List of found glitches:%u", found_glitches.count);
    return dyn_label2;
}

// активировать/деактивировать выбранный глитч
static void menu_item_activate_selected_glitch(void) {
    if (!found_glitches.virt_address[menu.current->selected_index]) {
        show_notification("Empty slot", RG_MSG_ERROR);
        return;
    }

    found_glitches.activate[menu.current->selected_index] = !found_glitches.activate[menu.current->selected_index];

    if(found_glitches.activate[menu.current->selected_index])
        show_notification("Selected glitch is ENABLED", RG_MSG_INFO);
    else
        show_notification("Selected glitch is DISABLED", RG_MSG_INFO);

    game_reset();
}

static void menu_item_open_options(void) { // открыть меню настроек
    menu.current = &menu.settings;
    menu_show();
}

static void menu_item_pause_effect(void) { // выбрать эффект при паузе
    pause_effect = (pause_effect + 1) % 5;
}

static void menu_item_open_list_of_found_glitches(void) { // открыть список найденных глитчей
    if (!found_glitches.virt_address[0]) {
        show_notification("Glitches NOT found ... yet", RG_MSG_ERROR);
        return;
    }
    menu.current = &menu.list;
    menu_show();
}

static uint32_t xorshift(uint32_t* seed) {
    uint32_t x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (*seed = x);
}

// перемешивание инструкций
static void shuffle_instructions(void) {
    static uint32_t seed = 19881029;
    seed ^= m68k_get_reg(M68K_REG_PC);
    seed ^= m68k_get_reg(M68K_REG_IR);
    seed ^= m68k_get_reg(M68K_REG_D2);

    //равномерное перемешивание
    for (uint32_t i = 0; i < rg_main.glitch_count - 1; i++) {
        uint32_t j = i + (xorshift(&seed) % (rg_main.glitch_count - i));
        rom_glitch_t temp = rg_main.glitches[i];
        rg_main.glitches[i] = rg_main.glitches[j];
        rg_main.glitches[j] = temp;
    }
}

// восстановить оригинальные значения инструкций
static void restore_instructions(void) {
    if (!rg_main.glitches) return;

    for (uint32_t i = rg_main.range_start; i < rg_main.range_start + rg_main.range_size && i < rg_main.glitch_count; i++)
        rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value;
}

// инвертирование инструкций
static void inversion_instructions(void) {
    for (uint32_t i = rg_main.range_start; i < rg_main.range_start + rg_main.range_size && i < rg_main.glitch_count; i++)
        rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value ^ 1;
}

static void menu_item_0_launch_glitcher(void) { // действие 0 "Launch Glitcher"
    if (rg_main.init_done) {
        menu_hide();
        game_save_state_to_ram();
        rg_main.launch = true;
        menu.main.selected_index = 0;
        menu.settings.selected_index = 0;
        menu.current = &menu.main;
        create_search_backup();
        shuffle_instructions();
        rg_main.step_count++;
        inversion_instructions();
        game_reset();
    }
    else
        show_notification("Candidates NOT found", RG_MSG_ERROR);
}

static void menu_item_1_bug_not_understand(void) { // действие 1 "Bug"
    create_search_backup();
    restore_instructions();

    if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
        if (rg_main.range_size == 1) {
            if (rg_main.localizing)
                show_notification("Glitch NOT found. Try again (#7)", RG_MSG_ERROR);
            else
                show_notification("Glitch NOT found. Try again (#6)", RG_MSG_ERROR);

            rg_reset();
            return;
        }
        else {
            rg_main.range_start = 0;
            rg_main.range_size -= rg_main.range_size / 2;

            if (!rg_main.localizing)
                shuffle_instructions();
        }
    }
    else
        rg_main.range_start += rg_main.range_size;

    rg_main.step_count++;
    inversion_instructions();
    game_reset();
}

static void menu_item_2_glitch_not_found(void) { // действие 2 "Not found"
    create_search_backup();
    restore_instructions();

    if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
        if (rg_main.range_size == 1 || rg_main.range_start == 0) {
            if (rg_main.localizing)
                show_notification("Glitch NOT found. Try again (#9)", RG_MSG_ERROR);
            else
                show_notification("Glitch NOT found. Try again (#8)", RG_MSG_ERROR);

            rg_reset();
            return;
        }
        else {
            rg_main.glitch_count = rg_main.range_start;
            rg_main.range_start = 0;
            rg_main.range_size -= rg_main.range_size / 2;

            if (!rg_main.localizing)
                shuffle_instructions();
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
    inversion_instructions();
    game_reset();
}

static void menu_item_3_glitch_found(void) { // действие 3 "Found"
    create_search_backup();

    if (!rg_main.localizing) {
        rg_main.localizing = true;
        //сохранение_поиска(); // нет реализации !!!
    }

    if (rg_main.range_size == 1 || rg_main.glitch_count == 1) {
        // глитч найден
        uint8_t idx;

        // поиск дубликата в уже найденных глитчах
        for (int i = 1; i < RG_GLITCH_SLOTS_MAX; i++) {
            if (rg_main.glitches[0].address == found_glitches.virt_address[i]) {
                show_notification("Duplicate. Glitch has already been found", RG_MSG_INFO);
                rg_reset();
                return;
            }
        }

        // выбор слота для добавления глитча в список найденных
        if (found_glitches.count < RG_GLITCH_SLOTS_MAX) {
            idx = found_glitches.count;
            found_glitches.count++;
        }
        else {
            for (int i = 1; i < RG_GLITCH_SLOTS_MAX; i++) {
                found_glitches.virt_address[i - 1] = found_glitches.virt_address[i];
                found_glitches.initial_value[i - 1] = found_glitches.initial_value[i];
                found_glitches.real_address[i - 1] = found_glitches.real_address[i];
                found_glitches.activate[i - 1] = found_glitches.activate[i];
            }

            idx = RG_GLITCH_SLOTS_MAX - 1;
            show_notification("Slots are full. Glitch #1 removed", RG_MSG_INFO);
        }

        found_glitches.initial_value[idx] = rg_main.glitches[0].initial_value;
        found_glitches.mod_value[idx] = rg_main.glitches[0].mod_value;
        found_glitches.activate[idx] = false;
        found_glitches.virt_address[idx] = rg_main.glitches[0].address;
        found_glitches.real_address[idx] = virt_rom_to_real_rom_offset(found_glitches.virt_address[idx]);
        add_glitch_as_cheat_to_file(found_glitches.virt_address[idx], found_glitches.real_address[idx],
            found_glitches.initial_value[idx], found_glitches.mod_value[idx]);

        char temp[84];
        snprintf(temp, sizeof(temp), "Glitch #%u. Steps %u, Real ROM '0x%06X' (VirtROM %06X)",
            found_glitches.count, rg_main.step_count, found_glitches.real_address[found_glitches.count - 1],
            found_glitches.virt_address[found_glitches.count - 1]);
        show_notification(temp, RG_MSG_FOUND);
        rg_reset();
        return;
    }
    else {
        restore_instructions();

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
    }

    rg_main.step_count++;
    inversion_instructions();
    game_reset();
}

static void menu_item_4_step_back(void) { // действие 4 "Step back"
    if (rg_backup_count == 0) {
        show_notification("No data from previous step", RG_MSG_ERROR);
        return;
    }

    rg_backup_index = (rg_backup_index - 1 + RG_BACKUP_SLOTS_MAX) % RG_BACKUP_SLOTS_MAX;
    rg_backup_count--;

    rom_glitcher_backup_t* slot = &rg_backups[rg_backup_index];

    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    rg_main = slot->backup;

    if (slot->glitches_backup && slot->backup.glitch_count > 0) {
        rg_main.glitches = malloc(sizeof(rom_glitch_t) * rg_main.glitch_count);
        if (rg_main.glitches) {
            memcpy(rg_main.glitches, slot->glitches_backup,
                sizeof(rom_glitch_t) * rg_main.glitch_count);
        }
        else {
            rg_main.glitches = NULL;
            rg_main.glitch_count = 0;
            show_notification("Step restore failed (memory)", RG_MSG_ERROR);
        }
    }
    else {
        rg_main.glitches = NULL;
        show_notification("Step restore failed (no backup)", RG_MSG_ERROR);
    }

    game_reset();
}

// "Отрисовка" меню
static void menu_show(void) {
    if (!menu_visible || !menu.current || !rg_cbs.environ_cb) 
        return;

    char menu_text[128] = {0}; // <= 128 !

    for (int i = 0; i < menu.current->item_count; i++) {
        const char* label = menu.current->items[i].get_label ? menu.current->items[i].get_label() : menu.current->items[i].label;

        if (label[0]) {
            char buf[64];
            snprintf(buf, sizeof(buf), " %s %s\n", (i == menu.current->selected_index) ? "<>" : " .  ", label);
            strcat(menu_text, buf);
        }
    }

    struct retro_message_ext msg = {
        .msg = menu_text,                   // текст сообщения <= 128 !
        .duration = 40000,                  // время отображения в мс
        .priority = 2,                      // приоритет очереди отображения
        .level = RETRO_LOG_DEBUG,           // уровень сообщения
        .target = RETRO_MESSAGE_TARGET_OSD, // только на экран
        .type = RETRO_MESSAGE_TYPE_STATUS,  // тип (в каком месте экрана выводится)
        .progress = -1                      // прогресс бар
    };

    rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);

    snprintf(log_text, sizeof(log_text), "Steps:%u (%s)  |  Candidates:%u/%u  |  %u.%u%%  |  Range start:%u size:%u",
        rg_main.step_count, rg_main.localizing ? "local" : "search", rg_main.glitch_count,
        rg_main.total_glitch_count, (rg_main.range_size > 0) ? ((rg_main.range_size * 1000) / rg_main.glitch_count) / 10 : 0,
        (rg_main.range_size > 0) ? ((rg_main.range_size * 1000) / rg_main.glitch_count) % 10 : 0, rg_main.range_start, rg_main.range_size);

    struct retro_message msg_under = { log_text, 2400 }; // {текст, время отображения в кадрах} (2400 = 40 сек * 60 Гц)
    rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg_under);
    memset(log_text, 0, sizeof(log_text));
}

// cкрыть меню
static void menu_hide(void) {
    menu_visible = false;
    if (rg_cbs.environ_cb) {
        struct retro_message clear_msg_under = { " ", 1};
        rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &clear_msg_under);

        struct retro_message_ext clear_msg = {
            .msg = "", .duration = 1, .priority = 10, .level = RETRO_LOG_DEBUG,
            .target = RETRO_MESSAGE_TARGET_OSD, .type = RETRO_MESSAGE_TYPE_STATUS,
            .progress = -1 };
        rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &clear_msg);
    }
}

// опрос кнопок геймпада и обработка ввода
void rg_input_processing(void)
{   
    if (need_load_state) {
        need_load_state = false;
        game_load_state_from_ram();
        return;
    }

    if (!rg_cbs.input_poll_cb || !rg_cbs.input_state_cb)
        return;

    rg_cbs.input_poll_cb();

    // 0:Menu, 1:Prev, 2:Next, 3:Confirm/Found, 4:Cancel/Bug, 5:NotFound, 6:StepBack
    static struct {
        bool was_pressed;
        bool is_processed;
    } button_states[7] = { 0 };

    bool menu_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, rg_menu_button);

    if (!menu_key && button_states[0].was_pressed && !button_states[0].is_processed) {
        menu_visible = !menu_visible;
        if (!menu_visible)
            menu_hide();
        else {
            menu.current = rg_main.launch ? &menu.main : &menu.launch;
            menu_show();
        }

        button_states[0].is_processed = true;
    }
    button_states[0].was_pressed = menu_key;
    if (menu_key) button_states[0].is_processed = false;

    if (menu_visible) {
        bool prev_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
        bool next_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
        bool confirm_found_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
        bool cancel_bug_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
        bool not_found_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
        bool step_back_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);

        if (!prev_key && button_states[1].was_pressed && !button_states[1].is_processed) {
            if (menu.current) {
                menu.current->selected_index =
                    (menu.current->selected_index - 1 + menu.current->item_count) % menu.current->item_count;
                menu_show();
            }
            button_states[1].is_processed = true;
        }
        button_states[1].was_pressed = prev_key;
        if (prev_key) button_states[1].is_processed = false;

        if (!next_key && button_states[2].was_pressed && !button_states[2].is_processed) {
            if (menu.current) {
                menu.current->selected_index = (menu.current->selected_index + 1) % menu.current->item_count;
                menu_show();
            }
            button_states[2].is_processed = true;
        }
        button_states[2].was_pressed = next_key;
        if (next_key) button_states[2].is_processed = false;

        if (!confirm_found_key && button_states[3].was_pressed && !button_states[3].is_processed) {
            if (menu.current == &menu.main && menu.current->selected_index == 0)
                menu_item_3_glitch_found();
            else if (menu.current) {
                rom_glitcher_menu_items_t* item = &menu.current->items[menu.current->selected_index];
                if (item->action) item->action();
                menu_show();
            }
            button_states[3].is_processed = true;
        }
        button_states[3].was_pressed = confirm_found_key;
        if (confirm_found_key) button_states[3].is_processed = false;

        if (!cancel_bug_key && button_states[4].was_pressed && !button_states[4].is_processed) {
            if (menu.current == &menu.settings || menu.current == &menu.list) {
                menu.current = rg_main.launch ? &menu.main : &menu.launch;
                menu_show();
            }
            else if (menu.current == &menu.main && menu.current->selected_index == 0)
                menu_item_1_bug_not_understand();
            else
                menu_hide();

            button_states[4].is_processed = true;
        }
        button_states[4].was_pressed = cancel_bug_key;
        if (cancel_bug_key) button_states[4].is_processed = false;

        if (!not_found_key && button_states[5].was_pressed && !button_states[5].is_processed) {
            if (menu.current == &menu.main && menu.current->selected_index == 0)
                menu_item_2_glitch_not_found();

            button_states[5].is_processed = true;
        }
        button_states[5].was_pressed = not_found_key;
        if (not_found_key) button_states[5].is_processed = false;

        if (!step_back_key && button_states[6].was_pressed && !button_states[6].is_processed) {
            if (menu.current == &menu.main && menu.current->selected_index == 0)
                menu_item_4_step_back();

            button_states[6].is_processed = true;
        }
        button_states[6].was_pressed = step_back_key;
        if (step_back_key) button_states[6].is_processed = false;
    }
    else {
        for (int i = 1; i < ARRAY_SIZE(button_states); i++) {
            button_states[i].was_pressed = false;
            button_states[i].is_processed = false;
        }
    }
}

static uint16_t get_rom_checksum(uint8* rom, int size) {
    uint16_t checksum = 0;

    for (int i = 0; i < size; i += 2)
        checksum += ((rom[i] << 8) + rom[i + 1]);

    return checksum;
}

static void apply_glitches(void) {
    // изменения в ROM при отсеивании кандидатов
    for (uint32_t i = 0; i < rg_main.glitch_count; i++)
        cart.rom[rg_main.glitches[i].address] = rg_main.glitches[i].mod_value;

    // применение активированных глитчей из списка найденных
    for (uint8_t i = 0; i < RG_GLITCH_SLOTS_MAX; i++)
        if (found_glitches.activate[i] && found_glitches.virt_address[i])
            cart.rom[found_glitches.virt_address[i]] = found_glitches.mod_value[i];

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
            show_notification("Checksum in header has been corrected", RG_MSG_INFO);
        }
    }

    for (int i = 0; i < RG_BACKUP_SLOTS_MAX; i++) {
        if (rg_backups[i].glitches_backup) {
            free(rg_backups[i].glitches_backup);
            rg_backups[i].glitches_backup = NULL;
        }
    }
    rg_backup_index = 0;
    rg_backup_count = 0;

    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    rg_main.init_done = false;
    rg_main.glitch_count = 0;
    rg_main.capacity = 1024;
    rg_main.glitches = malloc(rg_main.capacity * sizeof(rom_glitch_t));

    uint32_t trim = rom_has_header ? 0 : 0x200;

    if (rom_size <= trim) {
        show_notification("ROM too small", RG_MSG_ERROR);
        return;
    }

    // Читаем нормализованный ROM
    uint8_t high_byte = 0;
    uint8_t low_byte = 0;
    int32_t target_addr = 0;
    uint8_t target_high_byte = 0;
    uint8_t target_low_byte = 0;

    for (uint32_t byte_addr = trim; byte_addr + 1 < rom_size; byte_addr += 2) {
        high_byte = rom_data[byte_addr];

        // Ищем только BEQ(0x67) и BNE(0x66)
        if (high_byte != 0x66 && high_byte != 0x67) 
            continue;

        low_byte = rom_data[byte_addr + 1];

        // Проверка целевого адреса на попадание в ROM и чётность
        if (low_byte != 0 && (low_byte & 1) == 0) {
            // Короткое смещение
            target_addr = byte_addr + 2 + (int8_t)low_byte;
        }
        else {
            // Длинное смещение
            if (low_byte != 0)
                continue;

            if (byte_addr + 3 >= rom_size)
                continue;

            int16_t disp16 = (int16_t)((rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3]);

            if (disp16 == 0 || disp16 == 1)
                continue;

            target_addr = byte_addr + 2 + (int16_t)disp16;
        }

        if (target_addr < (int32_t)trim || target_addr >= (int32_t)rom_size) 
            continue;

        if (target_addr & 1) 
            continue;

        // Проверка данных по целевому адресу на легальность для M68K
        uint16_t target_opcode = (rom_data[target_addr] << 8) | rom_data[target_addr + 1];

        if ((target_opcode & 0b1111000000000000) == 0b1010000000000000 ||   // 1010xxxx xxxxxxxx
            (target_opcode & 0b1111000000000000) == 0b1111000000000000 ||   // 1111xxxx xxxxxxxx
            (target_opcode & 0b1111000100000000) == 0b0111000100000000)     // 0111xxx1 xxxxxxxx
            continue;

        // Правдоподобная BEQ/BNE инструкция
        if (rg_main.glitch_count >= rg_main.capacity) {
            rg_main.capacity *= 2;
            rg_main.glitches = realloc(rg_main.glitches,
                rg_main.capacity * sizeof(rom_glitch_t));
        }

        rg_main.glitches[rg_main.glitch_count].address = byte_addr;
        rg_main.glitches[rg_main.glitch_count].initial_value = high_byte;
        rg_main.glitches[rg_main.glitch_count].mod_value = high_byte;
        rg_main.glitch_count++;
    }

    /*if (rg_cbs.log_cb) {
        int offset = 0;
        for (int i = 0; i < ARRAY_SIZE(debug); i++)
            offset += snprintf(log_text + offset, sizeof(log_text) - offset, "*%u", debug[i]);

        offset += snprintf(log_text + offset, sizeof(log_text) - offset, "*%u", rg_main.glitch_count);
        rg_cbs.log_cb(RETRO_LOG_INFO, "\n\n%s\n\n\n", log_text);
    }*/
    
    need_load_state = false;
    menu.current = &menu.launch;
    rg_main.total_glitch_count = rg_main.glitch_count;
    rg_main.step_count = 0;
    rg_main.bug_step_count = 0;
    rg_main.localizing = false;
    rg_main.launch = false;
    rg_main.range_start = 0;
    rg_main.init_done = rg_main.glitch_count ? true : false;
    rg_main.range_size = (rg_main.total_glitch_count + 31) / 32; // примерно 3% от всех кандидатов

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "Candidates: %u%s", rg_main.glitch_count, rg_main.init_done ? "" : ", NOT found");
    show_notification(tmp, rg_main.init_done ? RG_MSG_INFO : RG_MSG_ERROR);
}

// сохранение предыдущего состояния отсеивания кандидатов
static void create_search_backup(void) {
    rom_glitcher_backup_t* slot = &rg_backups[rg_backup_index];

    if (slot->glitches_backup) {
        free(slot->glitches_backup);
        slot->glitches_backup = NULL;
    }

    slot->backup = rg_main;

    if (rg_main.glitch_count > 0 && rg_main.glitches) {
        slot->glitches_backup = malloc(sizeof(rom_glitch_t) * rg_main.glitch_count);
        if (slot->glitches_backup) {
            memcpy(slot->glitches_backup, rg_main.glitches,
                sizeof(rom_glitch_t) * rg_main.glitch_count);

            slot->backup.glitches = slot->glitches_backup;
        }
        else {
            slot->backup.glitches = NULL;
            slot->backup.glitch_count = 0;
            show_notification("Previous step NOT saved (memory)", RG_MSG_ERROR);
            return;
        }
    }
    else {
        slot->glitches_backup = NULL;
        show_notification("Previous step NOT saved (no data)", RG_MSG_ERROR);
        return;
    }

    rg_backup_index = (rg_backup_index + 1) % RG_BACKUP_SLOTS_MAX;
    if (rg_backup_count < RG_BACKUP_SLOTS_MAX)
        rg_backup_count++;
}

// сброс текущего поиска
static void rg_reset(void) { 
    restore_instructions();
    rg_main.init_done = false;
    game_reset();
    game_load_state_from_ram();
}

// очистка памяти при закрытии ядра
void rg_deinit(void) {
    menu_hide();
    menu.current = NULL; 
    
    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    for (int i = 0; i < RG_BACKUP_SLOTS_MAX; i++) {
        if (rg_backups[i].glitches_backup) {
            free(rg_backups[i].glitches_backup);
            rg_backups[i].glitches_backup = NULL;
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

// сохранения файла с читами в папку с ROM
static uint8_t add_glitch_as_cheat_to_file(uint32_t virt_address, uint32_t real_address, uint8_t initial_value, uint8_t mod_value) {
    char cheats_path[512] = { 0 };
    RFILE* f_cht = NULL;

#if defined(_WIN32)
    char slash = '\\';
#else
    char slash = '/';
#endif

    if (!rg_last_game || !rg_last_game->path) {
        show_notification("No last game info", RG_MSG_ERROR);
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
        return 2; // уже есть такой глитч в файле
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
        "Glitch #%u (0x%02X->0x%02X) steps %u", cheats_count, initial_value, mod_value, rg_main.step_count);

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
    f_cht = filestream_open(cheats_path,
        RETRO_VFS_FILE_ACCESS_WRITE,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);
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

    show_notification(tmp, RG_MSG_FOUND);

    return 0;
}

// вывод информационных сообщений на экран
static void show_notification(const char* s, uint8_t context) {
    char msg_text[256];
    uint32_t duration = 5000;
    // 0, 1, 2, 3 ... 255, обычно от 0 до 3
    uint8_t priority = 1; 
    // RETRO_LOG_DEBUG, RETRO_LOG_INFO, RETRO_LOG_WARN, RETRO_LOG_ERROR
    uint8_t level = RETRO_LOG_DEBUG;
    // RETRO_MESSAGE_TYPE_NOTIFICATION, RETRO_MESSAGE_TYPE_NOTIFICATION_ALT, RETRO_MESSAGE_TYPE_STATUS, RETRO_MESSAGE_TYPE_PROGRESS
    //uint8_t type = RETRO_MESSAGE_TYPE_NOTIFICATION;

    if (context == RG_MSG_INFO) {
        snprintf(msg_text, sizeof(msg_text), "RG: %s.", s);
        priority = 1;
        level = RETRO_LOG_INFO;
    }
    else if (context == RG_MSG_ERROR) {
        snprintf(msg_text, sizeof(msg_text), "RG ERROR: %s.", s);
        priority = 4;
        level = RETRO_LOG_ERROR;
    }
    else if (context == RG_MSG_FOUND) {
        snprintf(msg_text, sizeof(msg_text), "RG FOUND: %s.", s);
        duration = 15000;
        priority = 5;
        level = RETRO_LOG_INFO;
    }

    struct retro_message_ext msg = {
        .msg = msg_text,                         // текст сообщения
        .duration = duration,                    // время отображения в мс
        .priority = priority,                    // приоритет очереди отображения
        .level = level,                          // уровень сообщения (иконка сообщения)
        .target = RETRO_MESSAGE_TARGET_OSD,      // только на экран
        .type = RETRO_MESSAGE_TYPE_NOTIFICATION, // тип
        .progress = -1                     		 // прогресс бар
    };

    rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
}

// сброс игры
static void game_reset(void) {
    menu_hide();
    menu.current = NULL;
    retro_unload_game();
    retro_load_game(rg_last_game);
}

// сохранить save state игры в памяти для работы с глитчером
static void game_save_state_to_ram(void) {
    if (retro_serialize(game_state_buffer, sizeof(game_state_buffer)))
        show_notification("State saved", RG_MSG_INFO);
    else
        show_notification("State NOT saved", RG_MSG_ERROR);
}

// загрузить load state игры из памяти для работы с глитчером
static void game_load_state_from_ram(void) {
    menu_hide();

    if (!retro_unserialize(game_state_buffer, sizeof(game_state_buffer)))
        show_notification("State NOT loaded", RG_MSG_ERROR);
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
                    static uint32_t seed = 19881029;

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
                            uint8_t noise = xorshift(&seed) % 25;
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