// rom_glitcher_menu.c

#include "shared.h"

static const char* get_label_main(void);
static const char* get_label_list_of_found(void);
static const char* get_label_branch_allowed(void);
static const char* get_label_menu_list_of_found(void);
static const char* get_label_menu_branch_allowed(void);
static void menu_item_pause_effect(void);
static void menu_item_toggle_found_instruction(void);
static void menu_item_branch_allowed(void);
static void menu_item_prev_page(void);
static void menu_item_next_page(void);
static void menu_item_open_options(void);
static void menu_item_open_list_of_found(void);
static void menu_item_open_branch_allowed(void);
static void menu_item_game_save_state(void);
static void menu_item_launch_glitcher(void);

static bool refresh_menu_log = true;
static char dyn_label_main[64];
static char dyn_label_list_of_found[32];
static char dyn_label_branch_allowed[32];
static char dyn_label_menu_list_of_found[8][20];
static char dyn_label_menu_branch_allowed[8][20];

static rom_glitcher_menu_item_t menu_launch[] = {
    { "Launch Glitcher " RG_VERSION, NULL, menu_item_launch_glitcher },
    { "List of found", get_label_list_of_found, menu_item_open_list_of_found },
    { "Branch allowed", get_label_branch_allowed, menu_item_open_branch_allowed }
};

static rom_glitcher_menu_item_t menu_main[] = {
    { "BUG NOT_FOUND FOUND Step_back", get_label_main, NULL },
    { "List of found", get_label_list_of_found, menu_item_open_list_of_found },
    { "Stop Glitcher", NULL, rg_force_stop_glitcher },
    { "Options", NULL, menu_item_open_options }
};

static rom_glitcher_menu_item_t menu_options[] = {
    { "Save new state", NULL, menu_item_game_save_state },
    //{ "Load last state", NULL, rg_game_load_state },
    { "Pause effect", NULL, menu_item_pause_effect }
};

static rom_glitcher_menu_item_t menu_list[] = {
    { "prev", get_label_menu_list_of_found, menu_item_prev_page },
    { "1", get_label_menu_list_of_found, menu_item_toggle_found_instruction },
    { "2", get_label_menu_list_of_found, menu_item_toggle_found_instruction },
    { "3", get_label_menu_list_of_found, menu_item_toggle_found_instruction },
    { "4", get_label_menu_list_of_found, menu_item_toggle_found_instruction },
    { "5", get_label_menu_list_of_found, menu_item_toggle_found_instruction },
    { "6", get_label_menu_list_of_found, menu_item_toggle_found_instruction },
    { "next", get_label_menu_list_of_found, menu_item_next_page }
};

static rom_glitcher_menu_item_t menu_branch[] = {
    { "- -", NULL, NULL },
    //{ "60/61", get_label_menu_branch_allowed, menu_item_branch_allowed }, // BRA/BSR
    { "62/63", get_label_menu_branch_allowed, menu_item_branch_allowed }, // BHI/BLS
    { "64/65", get_label_menu_branch_allowed, menu_item_branch_allowed }, // BCC/BCS
    { "66/67", get_label_menu_branch_allowed, menu_item_branch_allowed }, // BNE/BEQ
    { "68/69", get_label_menu_branch_allowed, menu_item_branch_allowed }, // BVC/BVS
    { "6A/6B", get_label_menu_branch_allowed, menu_item_branch_allowed }, // BPL/BMI
    { "6C/6D", get_label_menu_branch_allowed, menu_item_branch_allowed }, // BGE/BLT
    { "6E/6F", get_label_menu_branch_allowed, menu_item_branch_allowed }  // BGT/BLE
};

rom_glitcher_menu_manager_t rg_menu = {
    .launch = { menu_launch, ARRAY_SIZE(menu_launch), 0 },
    .main = { menu_main, ARRAY_SIZE(menu_main), 0 },
    .options = { menu_options, ARRAY_SIZE(menu_options), 0 },
    .list = { menu_list, ARRAY_SIZE(menu_list), 1 },
    .branch = { menu_branch, ARRAY_SIZE(menu_branch), 3 }
};

static const char* get_label_main(void) {
    snprintf(dyn_label_main, sizeof(dyn_label_main), "[%s]Bug [%s]NOT found [%s]Found [%s]Step back:%u/%u",
        rg_button_states[4].was_pressed ? "#" : "B",
        rg_button_states[5].was_pressed ? "#" : "X",
        rg_button_states[3].was_pressed ? "#" : "A",
        rg_button_states[6].was_pressed ? "#" : "Y",
        rg_backup_count, RG_MAX_BACKUP_SLOTS);
    return dyn_label_main;
}

static const char* get_label_list_of_found(void) {
    snprintf(dyn_label_list_of_found, sizeof(dyn_label_list_of_found), "List of found: %u/%u",
        rg_found_glitches.enabled_count, rg_found_glitches.count);
    return dyn_label_list_of_found;
}

static const char* get_label_branch_allowed(void) {
    uint8_t pairs_count = ((rg_branch_allowed >> 0) & 1) + ((rg_branch_allowed >> 1) & 1)
        + ((rg_branch_allowed >> 2) & 1) + ((rg_branch_allowed >> 3) & 1)
        + ((rg_branch_allowed >> 4) & 1) + ((rg_branch_allowed >> 5) & 1)
        + ((rg_branch_allowed >> 6) & 1) + ((rg_branch_allowed >> 7) & 1);
    snprintf(dyn_label_branch_allowed, sizeof(dyn_label_branch_allowed), "Branch pairs allowed: %u/8", pairs_count);
    return dyn_label_branch_allowed;
}

static const char* get_label_menu_list_of_found(void) {
    static uint8_t j = 0;
    uint8_t row = j;
    j = (j + 1) % 8;

    if (row == 0) { // prev page
        uint8_t prev_page = (rg_found_glitches.total_pages == 0) ? 0 :
            (rg_found_glitches.current_page + rg_found_glitches.total_pages - 1) % rg_found_glitches.total_pages;

        snprintf(dyn_label_menu_list_of_found[row], sizeof(dyn_label_menu_list_of_found[row]),
            "cur %u/%s %u", rg_found_glitches.current_page + 1, menu_list[row].label,
            (rg_found_glitches.total_pages > 0) ? (prev_page + 1) : 0);
        return dyn_label_menu_list_of_found[row];
    }

    if (row > 0 && row < 7) {
        uint16_t index = rg_found_glitches.current_page * RG_MAX_FOUND_GLITCH_PER_PAGE + (row - 1);
        if (index < rg_found_glitches.count && rg_found_glitches.real_address[index]) {
            snprintf(dyn_label_menu_list_of_found[row], sizeof(dyn_label_menu_list_of_found[row]), "%06X %s",
                rg_found_glitches.real_address[index],
                rg_found_glitches.enabled[index] ? "ON" : "OFF");
        }
        else {
            snprintf(dyn_label_menu_list_of_found[row], sizeof(dyn_label_menu_list_of_found[row]), " - - -");
        }

        return dyn_label_menu_list_of_found[row];
    }

    if (row == 7) { // next page
        uint8_t next_page = (rg_found_glitches.total_pages == 0) ? 0 :
            (rg_found_glitches.current_page + 1) % rg_found_glitches.total_pages;

        snprintf(dyn_label_menu_list_of_found[row], sizeof(dyn_label_menu_list_of_found[row]),
            "%s %u", menu_list[row].label,
            (rg_found_glitches.total_pages > 0) ? (next_page + 1) : 0);
        return dyn_label_menu_list_of_found[row];
    }

    snprintf(dyn_label_menu_list_of_found[row], sizeof(dyn_label_menu_list_of_found[row]), " ");
    return dyn_label_menu_list_of_found[row];
}

static const char* get_label_menu_branch_allowed(void) {
    static uint8_t j = 0;
    uint8_t row = j;
    j = (j + 1) % 8;

    snprintf(dyn_label_menu_branch_allowed[row], sizeof(dyn_label_menu_branch_allowed[row]), "%s %s",
        menu_branch[row].label, rg_branch_allowed & (1 << row) ? "ON" : "OFF");
    return dyn_label_menu_branch_allowed[row];
}

static void menu_item_pause_effect(void) {
    rg_pause_effect = (rg_pause_effect + 1) % 5;
}

static void menu_item_toggle_found_instruction(void) {
    uint16_t index = rg_found_glitches.current_page * RG_MAX_FOUND_GLITCH_PER_PAGE 
        + (rg_menu.current->selected_index - 1);

    if (index >= rg_found_glitches.count || !rg_found_glitches.virt_address[index]) {
        rg_msg("Empty slot", RG_MSG_ERROR);
        return;
    }

    rg_found_glitches.enabled[index] = !rg_found_glitches.enabled[index];
    rg_found_glitches_modified = true;
}

static void menu_item_branch_allowed(void) {
    rg_branch_allowed ^= (1 << rg_menu.current->selected_index);
    rg_game_save_state();
    rg_main.init_done = false;
    rg_game_reset();
    rg_game_load_state();
    rg_menu_visible = true;
    rg_menu.current = &rg_menu.branch;
}

static void menu_item_prev_page(void) {
    rg_found_glitches.current_page = (rg_found_glitches.current_page +
        rg_found_glitches.total_pages - 1) % rg_found_glitches.total_pages;
}

static void menu_item_next_page(void) {
    rg_found_glitches.current_page = (rg_found_glitches.current_page + 1) % rg_found_glitches.total_pages;
}

static void menu_item_open_options(void) { rg_menu.current = &rg_menu.options; }
static void menu_item_open_list_of_found(void) { rg_menu.current = &rg_menu.list; }
static void menu_item_open_branch_allowed(void) { rg_menu.current = &rg_menu.branch; }

static void menu_item_game_save_state(void) {
    rg_game_save_state();
    rg_input_replay.record = true;
    rg_input_replay.play = false;
    rg_input_replay.length = 0;
}

static void menu_item_launch_glitcher(void) {
    if (rg_main.init_done) {
        rg_game_save_state();
        rg_main.launch = true;
        rg_menu.main.selected_index = 0;
        rg_menu.options.selected_index = 0;
        rg_menu.current = &rg_menu.main;
        rg_create_step_backup();
        rg_instructions_shuffle();
        rg_main.step_count++;
        rg_instructions_inversion();
        rg_input_replay.record = true;
        rg_input_replay.play = false;
        rg_input_replay.length = 0;
    }
    else
        rg_msg("Candidates NOT found", RG_MSG_ERROR);
}

void menu_item_1_bug(void) {
    rg_create_step_backup();
    rg_instructions_restore();

    if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
        if (rg_main.range_size == 1) {
            if (rg_main.localizing)
                rg_msg("Glitch NOT found. Try again (#7)", RG_MSG_ERROR);
            else
                rg_msg("Glitch NOT found. Try again (#6)", RG_MSG_ERROR);

            rg_current_search_end();
            return;
        }
        else {
            rg_main.range_start = 0;
            rg_main.range_size -= rg_main.range_size / 2;

            if (!rg_main.localizing)
                rg_instructions_shuffle();
        }
    }
    else
        rg_main.range_start += rg_main.range_size;

    rg_main.step_count++;
    rg_instructions_inversion();
    rg_game_reset();
}

void menu_item_2_not_found(void) {
    rg_create_step_backup();
    rg_instructions_restore();

    if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
        if (rg_main.range_size == 1 || rg_main.range_start == 0) {
            if (rg_main.localizing)
                rg_msg("Glitch NOT found. Try again (#9)", RG_MSG_ERROR);
            else
                rg_msg("Glitch NOT found. Try again (#8)", RG_MSG_ERROR);

            rg_current_search_end();
            return;
        }
        else {
            rg_main.glitch_count = rg_main.range_start;
            rg_main.range_start = 0;
            rg_main.range_size -= rg_main.range_size / 2;

            if (!rg_main.localizing)
                rg_instructions_shuffle();
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
    rg_instructions_inversion();
    rg_game_reset();
}

void menu_item_3_found(void) {

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

    rg_create_step_backup();

    if (rg_main.range_size == 1 || rg_main.glitch_count == 1) {
        // глитч найден

        // поиск дубликата в уже найденных глитчах
        for (int i = 0; i < rg_found_glitches.count; i++) {
            if (rg_main.glitches[0].address == rg_found_glitches.virt_address[i]) {
                rg_current_search_end();
                return;
            }
        }

        // выбор слота для добавления глитча в список найденных
        if (rg_found_glitches.count >= RG_MAX_FOUND_GLITCH_SLOTS) {
            for (int i = 1; i < RG_MAX_FOUND_GLITCH_SLOTS; i++) {
                rg_found_glitches.virt_address[i - 1] = rg_found_glitches.virt_address[i];
                rg_found_glitches.initial_value[i - 1] = rg_found_glitches.initial_value[i];
                rg_found_glitches.real_address[i - 1] = rg_found_glitches.real_address[i];
                rg_found_glitches.enabled[i - 1] = rg_found_glitches.enabled[i];
            }

            rg_found_glitches.count = RG_MAX_FOUND_GLITCH_SLOTS - 1;
            rg_msg("Slots are full. First glitch removed", RG_MSG_INFO);
        }

        rg_found_glitches.initial_value[rg_found_glitches.count] = rg_main.glitches[0].initial_value;
        rg_found_glitches.mod_value[rg_found_glitches.count] = rg_main.glitches[0].mod_value;
        rg_found_glitches.enabled[rg_found_glitches.count] = true;
        rg_found_glitches.virt_address[rg_found_glitches.count] = rg_main.glitches[0].address;
        rg_found_glitches.real_address[rg_found_glitches.count] = rg_virt_to_real_rom_offset(rg_found_glitches.virt_address[rg_found_glitches.count]);
        rg_save_found_to_file(rg_found_glitches.virt_address[rg_found_glitches.count], rg_found_glitches.real_address[rg_found_glitches.count], rg_found_glitches.initial_value[rg_found_glitches.count], rg_found_glitches.mod_value[rg_found_glitches.count]);

        rg_found_glitches.count++;
        rg_found_glitches.total_pages =
            (rg_found_glitches.count + RG_MAX_FOUND_GLITCH_PER_PAGE - 1) / RG_MAX_FOUND_GLITCH_PER_PAGE;

        char temp[128];
        snprintf(temp, sizeof(temp), "Glitch %u. Steps %u. Real ROM '0x%06X'. Virt ROM '%06X'",
            rg_found_glitches.count, rg_main.step_count, rg_found_glitches.real_address[rg_found_glitches.count - 1],
            rg_found_glitches.virt_address[rg_found_glitches.count - 1]);
        rg_msg(temp, RG_MSG_FOUND);
        rg_current_search_end();
    }
    else {
        rg_instructions_restore();

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
        rg_instructions_inversion();
        rg_game_reset();
    }
}

void menu_item_4_step_back(void) { // действие 4 "Step back"
    if (rg_backup_count == 0)
        rg_msg("No data from previous step", RG_MSG_ERROR);
    else {
        rg_backup_index = (rg_backup_index - 1 + RG_MAX_BACKUP_SLOTS) % RG_MAX_BACKUP_SLOTS;
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
                rg_msg("Previous step restore failed (memory)", RG_MSG_ERROR);
            }
        }
        else {
            rg_main.glitches = NULL;
            rg_msg("Previous step restore failed (no backup)", RG_MSG_ERROR);
        }

        rg_game_reset();
    }
}

// "Отрисовка" меню
void rg_menu_show(void) {
    static uint8_t frame_skip;

    if (!rg_menu_visible || !rg_menu.current || !environ_cb || frame_skip % 3 != 0) {
        frame_skip++;
        return;
    }
    frame_skip++;

    char menu_text[128] = { 0 };

    for (int i = 0; i < rg_menu.current->item_count; i++) {
        const char* label = rg_menu.current->items[i].get_label ? rg_menu.current->items[i].get_label() : rg_menu.current->items[i].label;

        if (label && label[0]) {
            char buf[80];
            snprintf(buf, sizeof(buf), "%s %s\n", (i == rg_menu.current->selected_index) ? "<>" : "  . ", label);
            strncat(menu_text, buf, sizeof(menu_text) - strlen(menu_text) - 1);
        }
    }

    uint8_t len = strlen(menu_text);
    while (len < 128)
        menu_text[len++] = ' ';

    struct retro_message_ext msg = {
        .msg = menu_text,                   // текст сообщения < 128 !
        .duration = 100,                    // время отображения в мс
        .priority = 2,                      // приоритет очереди отображения
        .level = RETRO_LOG_INFO,            // уровень сообщения
        .target = RETRO_MESSAGE_TARGET_OSD, // только на экран
        .type = RETRO_MESSAGE_TYPE_STATUS,  // тип (в каком месте экрана выводится)
        .progress = -1                      // прогресс бар
    };

    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);

    if (refresh_menu_log) {
        refresh_menu_log = false;
        char log_text[164];
        char branch_mode[64] = { 0 };

        uint32_t percent = (rg_main.range_size * 1000) / rg_main.glitch_count;

        for (int i = 0; i < 8; i++)
            if (rg_branch_allowed & (1 << i))
                snprintf(branch_mode + strlen(branch_mode), sizeof(branch_mode) - strlen(branch_mode), "%s ", menu_branch[i].label);

        snprintf(log_text, sizeof(log_text), "Steps:%u (%s)  |  Candidates:%u/%u  |  %u.%u%%  |  Range start:%u size:%u  |  %s",
            rg_main.step_count, rg_main.localizing ? "LOCAL" : "SEARCH", rg_main.glitch_count, rg_total_glitch_count,
            (rg_main.range_size > 0) ? percent / 10 : 0, (rg_main.range_size > 0) ? percent % 10 : 0,
            rg_main.range_start, rg_main.range_size, branch_mode);
        struct retro_message msg_log = { log_text, 100 * rg_fps }; // {текст, время отображения в кадрах}
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg_log);
    }
}

// cкрыть меню
void rg_menu_hide(void) {
    if (environ_cb) {
        struct retro_message clear_msg_log = { " ", 1 };
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &clear_msg_log);
    }

    rg_menu_visible = false;
    refresh_menu_log = true;
}

// вывод информационных сообщений на экран
void rg_msg(const char* s, uint8_t context) {
    char msg_text[128];
    uint32_t duration = 4000;
    uint8_t priority = 1; // 0, 1, 2, 3 ... 255, обычно от 0 до 3
    uint8_t level = RETRO_LOG_DEBUG; // RETRO_LOG_DEBUG, RETRO_LOG_INFO, RETRO_LOG_WARN, RETRO_LOG_ERROR
    // RETRO_MESSAGE_TYPE_NOTIFICATION, RETRO_MESSAGE_TYPE_NOTIFICATION_ALT, RETRO_MESSAGE_TYPE_STATUS, RETRO_MESSAGE_TYPE_PROGRESS
    uint8_t type = RETRO_MESSAGE_TYPE_NOTIFICATION;
    int8_t progress = -1;

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
        duration = 10000;
        priority = 5;
        level = RETRO_LOG_INFO;
    }
    else if (context == RG_MSG_REPLAY_PLAY) {
        snprintf(msg_text, sizeof(msg_text), "RG REPLAY: PLAYBACK ('Menu' to stop). Steps %d [%s]",
            rg_main.step_count, rg_main.localizing ? "L" : "S");
        duration = 33;
        priority = 6;
        level = RETRO_LOG_INFO;
        type = RETRO_MESSAGE_TYPE_PROGRESS;
        progress = atoi(s);
    }
    else if (context == RG_MSG_REPLAY_REC) {
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
