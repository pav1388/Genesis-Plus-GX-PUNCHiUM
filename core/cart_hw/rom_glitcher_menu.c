// rom_glitcher_menu.c

#include "shared.h"

static const char* get_label_launch_glitcher(uint8_t index);
static const char* get_label_stop_glitcher(uint8_t index);
static const char* get_label_options(uint8_t index);
static const char* get_label_game_save_state(uint8_t index);
static const char* get_label_pause_effect(uint8_t index);
static const char* get_label_search(uint8_t index);
static const char* get_label_list_of_found(uint8_t index);
static const char* get_label_branch_allowed(uint8_t index);
static const char* get_label_menu_list_of_found(uint8_t index);
static const char* get_label_menu_branch_allowed(uint8_t index);
static void menu_item_pause_effect(void);
static void menu_item_toggle_found_instruction(void);
static void menu_item_branch_allowed(void);
static void menu_item_prev_page(void);
static void menu_item_next_page(void);
static void menu_item_open_options(void);
static void menu_item_open_list_of_found(void);
static void menu_item_open_branch_allowed(void);
static void menu_item_game_save_state(void);

static bool refresh_menu_log = true;
static char dyn_label_search[128];
static char dyn_label_list_of_found[128];
static char dyn_label_branch_allowed[128];
static char dyn_label_menu_list_of_found[8][20];
static char dyn_label_menu_branch_allowed[8][20];

static rom_glitcher_menu_item_t menu_launch[] = {
    { get_label_launch_glitcher, rg_launch_glitcher},           // Launch Glitcher
    { get_label_list_of_found, menu_item_open_list_of_found },  // List of found
    { get_label_branch_allowed, menu_item_open_branch_allowed } // Branch allowed
};

static rom_glitcher_menu_item_t menu_search[] = {
    { get_label_search, NULL },                                 // BUG NOT_FOUND FOUND Step_back
    { get_label_list_of_found, menu_item_open_list_of_found },  // List of found
    { get_label_stop_glitcher, rg_force_stop_glitcher },        // Stop Glitcher
    { get_label_options, menu_item_open_options }               // Options
};

static rom_glitcher_menu_item_t menu_options[] = {
    { get_label_game_save_state, menu_item_game_save_state },   // Save state
    { get_label_pause_effect, menu_item_pause_effect }          // Pause effect
};

static rom_glitcher_menu_item_t menu_found[] = {
    { get_label_menu_list_of_found, menu_item_prev_page },             // prev
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // 1
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // 2
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // 3
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // 4
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // 5
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // 6
    { get_label_menu_list_of_found, menu_item_next_page }              // next
};

static rom_glitcher_menu_item_t menu_branch[] = {
    { get_label_menu_branch_allowed, NULL },                       // BRA/BSR 0x60/61
    { get_label_menu_branch_allowed, menu_item_branch_allowed },   // BHI/BLS 0x62/63
    { get_label_menu_branch_allowed, menu_item_branch_allowed },   // BCC/BCS 0x64/65
    { get_label_menu_branch_allowed, menu_item_branch_allowed },   // BNE/BEQ 0x66/67
    { get_label_menu_branch_allowed, menu_item_branch_allowed },   // BVC/BVS 0x68/69
    { get_label_menu_branch_allowed, menu_item_branch_allowed },   // BPL/BMI 0x6A/6B
    { get_label_menu_branch_allowed, menu_item_branch_allowed },   // BGE/BLT 0x6C/6D
    { get_label_menu_branch_allowed, menu_item_branch_allowed }    // BGT/BLE 0x6E/6F
};

rom_glitcher_menu_manager_t rg_menu = {
    .current = NULL,
    .launch = { menu_launch, ARRAY_SIZE(menu_launch), 0 },
    .main = { menu_search, ARRAY_SIZE(menu_search), 0 },
    .options = { menu_options, ARRAY_SIZE(menu_options), 0 },
    .found = { menu_found, ARRAY_SIZE(menu_found), 1 },
    .branch = { menu_branch, ARRAY_SIZE(menu_branch), 3 }
};

static const char* get_label_search(uint8_t index) {
    snprintf(dyn_label_search, sizeof(dyn_label_search), TR(RG_TR_SEARCH_HEADER),
        rg_button_states[4].was_pressed ? "#" : "B",
        rg_button_states[5].was_pressed ? "#" : "X",
        rg_button_states[3].was_pressed ? "#" : "A",
        rg_button_states[6].was_pressed ? "#" : "Y");
    return dyn_label_search;
}

static const char* get_label_launch_glitcher(uint8_t index) { return TR(RG_TR_LAUNCH_GLITCHER); }
static const char* get_label_stop_glitcher(uint8_t index) { return TR(RG_TR_STOP_GLITCHER); }
static const char* get_label_options(uint8_t index) { return TR(RG_TR_OPTIONS); }
static const char* get_label_game_save_state(uint8_t index) { return TR(RG_TR_SAVE_STATE); }
static const char* get_label_pause_effect(uint8_t index) { return TR(RG_TR_PAUSE_EFFECT); }

static const char* get_label_list_of_found(uint8_t index) {
    snprintf(dyn_label_list_of_found, sizeof(dyn_label_list_of_found), "%s %u/%u", TR(RG_TR_LIST_OF_FOUND),
        rg_found_glitches.enabled_count, rg_found_glitches.count);
    return dyn_label_list_of_found;
}

static const char* get_label_branch_allowed(uint8_t index) {
    snprintf(dyn_label_branch_allowed, sizeof(dyn_label_branch_allowed), "%s %u/8", 
        TR(RG_TR_BRANCH_ALLOWED), 
        (uint8_t)__builtin_popcount(rg_branch_allowed));
    return dyn_label_branch_allowed;
}

static const char* get_label_menu_list_of_found(uint8_t index) {
    if (index == 0) { // prev page
        uint8_t prev_page = (rg_found_glitches.total_pages == 0) ? 0 :
            (rg_found_glitches.current_page + rg_found_glitches.total_pages - 1) % rg_found_glitches.total_pages;

        snprintf(dyn_label_menu_list_of_found[index], sizeof(dyn_label_menu_list_of_found[index]),
            TR(RG_TR_PREV_PAGE), rg_found_glitches.current_page + 1,
            (rg_found_glitches.total_pages > 0) ? (prev_page + 1) : 0);
        return dyn_label_menu_list_of_found[index];
    }

    if (index > 0 && index < 7) {
        uint16_t i = rg_found_glitches.current_page * RG_MAX_FOUND_GLITCH_PER_PAGE + (index - 1);
        if (i < rg_found_glitches.count && rg_found_glitches.real_address[i]) {
            snprintf(dyn_label_menu_list_of_found[index], sizeof(dyn_label_menu_list_of_found[index]), "%06X %s",
                rg_found_glitches.real_address[i],
                rg_found_glitches.enabled[i] ? TR(RG_TR_ON) : TR(RG_TR_OFF));
        }
        else {
            snprintf(dyn_label_menu_list_of_found[index], sizeof(dyn_label_menu_list_of_found[index]), " - - -");
        }

        return dyn_label_menu_list_of_found[index];
    }

    if (index == 7) { // next page
        uint8_t next_page = (rg_found_glitches.total_pages == 0) ? 0 :
            (rg_found_glitches.current_page + 1) % rg_found_glitches.total_pages;

        snprintf(dyn_label_menu_list_of_found[index], sizeof(dyn_label_menu_list_of_found[index]),
            TR(RG_TR_NEXT_PAGE), (rg_found_glitches.total_pages > 0) ? (next_page + 1) : 0);
        return dyn_label_menu_list_of_found[index];
    }

    return "";
}

static const char* get_label_menu_branch_allowed(uint8_t index) {
    static const char* branch_items[] = {
        //"60/61", "62/63", "64/65", "66/67", "68/69", "6A/6B", "6C/6D", "6E/6F"
        "BRA/BSR", "BHI/BLS", "BCC/BCS", "BNE/BEQ", "BVC/BVS", "BPL/BMI", "BGE/BLT", "BGT/BLE"
    };

    const char* status = (rg_branch_allowed & (1 << index)) ? TR(RG_TR_ON) : TR(RG_TR_OFF);

    snprintf(dyn_label_menu_branch_allowed[index], sizeof(dyn_label_menu_branch_allowed[index]),
        "%s %s", branch_items[index], status);

    return dyn_label_menu_branch_allowed[index];
}

static void menu_item_pause_effect(void) {
    rg_pause_effect = (rg_pause_effect + 1) % 5;
}

static void menu_item_toggle_found_instruction(void) {
    uint16_t index = rg_found_glitches.current_page * RG_MAX_FOUND_GLITCH_PER_PAGE
        + (rg_menu.current->selected_index - 1);

    if (index >= rg_found_glitches.count || !rg_found_glitches.virt_address[index])
        return;

    rg_found_glitches.enabled[index] = !rg_found_glitches.enabled[index];

    if (rg_found_glitches.enabled[index])
        rg_found_glitches.enabled_count++;
    else
        rg_found_glitches.enabled_count =
        rg_found_glitches.enabled_count > 0 ? rg_found_glitches.enabled_count - 1 : 0;

    // Обновляем файл с читами
    char cheats_path[512] = { 0 };
    RFILE* f_cht = NULL;

#if defined(_WIN32)
    char slash = '\\';
#else
    char slash = '/';
#endif

    snprintf(cheats_path, sizeof(cheats_path), "%s%c%s_RG.cht", g_rom_dir, slash, g_rom_name);
    f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!f_cht) {
        rg_found_glitches_modified = true;
        return; // Файл не существует
    }

    filestream_seek(f_cht, 0, RETRO_VFS_SEEK_POSITION_END);
    int32_t size = filestream_tell(f_cht);
    filestream_rewind(f_cht);

    if (size <= 0) {
        filestream_close(f_cht);
        rg_found_glitches_modified = true;
        return; // Пустой файл
    }

    char* file_content = (char*)calloc(1, size + 1);
    filestream_read(f_cht, file_content, size);
    filestream_close(f_cht);

    // Ищем адрес в файле
    char search[32];
    snprintf(search, sizeof(search), "%06X:%02X", rg_found_glitches.real_address[index], rg_found_glitches.mod_value[index]);
    char* found_pos = strstr(file_content, search);

    if (found_pos) {
        // Ищем начало блока с этим читом (ищем номер чита)
        char* block_start = found_pos;
        while (block_start > file_content) {
            if (strstr(block_start, "cheat") && strstr(block_start, "_code")) {
                break;
            }
            block_start--;
        }

        // Извлекаем номер чита
        if (block_start > file_content && strstr(block_start, "cheat")) {
            char* num_start = strstr(block_start, "cheat") + 5;
            int cheat_num = atoi(num_start);

            if (cheat_num > 0) {
                // Ищем строку enable для этого чита
                char enable_pattern[32];
                snprintf(enable_pattern, sizeof(enable_pattern), "cheat%d_enable = \"", cheat_num);
                char* enable_pos = strstr(file_content, enable_pattern);

                if (enable_pos) {
                    char* value_start = enable_pos + strlen(enable_pattern);
                    char* value_end = strchr(value_start, '"');

                    if (value_end) {
                        // Создаем новый файл с измененным значением
                        size_t prefix_len = value_start - file_content;
                        size_t suffix_len = strlen(value_end + 1);

                        char* new_file = (char*)malloc(prefix_len + 6 + suffix_len + 1);

                        memcpy(new_file, file_content, prefix_len);
                        if (rg_found_glitches.enabled[index])
                            strcpy(new_file + prefix_len, "true");
                        else
                            strcpy(new_file + prefix_len, "false");

                        strcpy(new_file + prefix_len + (rg_found_glitches.enabled[index] ? 4 : 5), value_end);

                        // Сохраняем измененный файл
                        f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
                        if (f_cht) {
                            filestream_write(f_cht, new_file, strlen(new_file));
                            filestream_close(f_cht);
                        }

                        free(new_file);
                    }
                }
            }
        }
    }

    free(file_content);
    rg_found_glitches_modified = true;
}

static void menu_item_branch_allowed(void) {
    rg_branch_allowed ^= (1 << rg_menu.current->selected_index);
    rg_game_save_state();
    rg_main.init_done = false;
    rg_clear_bug_range = true;
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
static void menu_item_open_list_of_found(void) { rg_menu.current = &rg_menu.found; }
static void menu_item_open_branch_allowed(void) { rg_menu.current = &rg_menu.branch; }

static void menu_item_game_save_state(void) {
    rg_game_save_state();
    rg_input_replay.record = true;
    rg_input_replay.play = false;
    rg_input_replay.length = 0;
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

    for (uint8_t i = 0; i < rg_menu.current->item_count; i++) {
        const char* label = rg_menu.current->items[i].get_label(i);
        char buf[128];

        if (label && label[0]) {
            snprintf(buf, sizeof(buf), "%s %s\n", (i == rg_menu.current->selected_index) ? "<>" : " . ", label);
            strncat(menu_text, buf, sizeof(menu_text) - strlen(menu_text) - 1);
        }
        else {
            snprintf(buf, sizeof(buf), "%s\n", (i == rg_menu.current->selected_index) ? "<>" : " . ");
            strncat(menu_text, buf, sizeof(menu_text) - strlen(menu_text) - 1);
        }
    }

    uint8_t len = strlen(menu_text);
    while (len < 125)
        menu_text[len++] = ' '; // чтобы меню было по левому краю

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
        char branch_mode[128] = { 0 };

        uint32_t percent = (rg_main.range_size * 1000) / rg_main.glitch_count;

        static const char* branch_items[] = {
             "BRA/BSR" ,
             "BHI/BLS" ,
             "BCC/BCS" ,
             "BNE/BEQ" ,
             "BVC/BVS" ,
             "BPL/BMI" ,
             "BGE/BLT" ,
             "BGT/BLE"
        };

        for (int i = 0; i < 8; i++)
            if (rg_branch_allowed & (1 << i))
                snprintf(branch_mode + strlen(branch_mode), sizeof(branch_mode) - strlen(branch_mode), "%s ", branch_items[i]);

        snprintf(log_text, sizeof(log_text), "%s %u (%s)  |  %s %u/%u  |  %u.%u%%  |  %s %u:%u  |  %s",
            TR(RG_TR_STEP),
            rg_main.step_count, rg_bug_glitches.count ? TR(RG_TR_BUG) : rg_main.localizing ? TR(RG_TR_LOCAL) : TR(RG_TR_SEARCH),
            TR(RG_TR_CANDIDATES),
            rg_main.glitch_count, rg_total_glitch_count,
            (rg_main.range_size > 0) ? percent / 10 : 0, (rg_main.range_size > 0) ? percent % 10 : 0,
            TR(RG_TR_RANGE),
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
void rg_msg(uint8_t context, const char* format, ...) {
    char message[256];
    char msg_text[256];
    uint32_t duration = 4000;
    uint8_t priority = 1; // 0, 1, 2, 3 ... 255, обычно от 0 до 3
    uint8_t level = RETRO_LOG_DEBUG; // RETRO_LOG_DEBUG, RETRO_LOG_INFO, RETRO_LOG_WARN, RETRO_LOG_ERROR
    // RETRO_MESSAGE_TYPE_NOTIFICATION, RETRO_MESSAGE_TYPE_NOTIFICATION_ALT, RETRO_MESSAGE_TYPE_STATUS, RETRO_MESSAGE_TYPE_PROGRESS
    uint8_t type = RETRO_MESSAGE_TYPE_NOTIFICATION;
    int8_t progress = -1;

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    switch (context) {
        case RG_MSG_INFO: {
            snprintf(msg_text, sizeof(msg_text), TR(RG_TR_RG_INFO), message);
            level = RETRO_LOG_INFO;
            break;
        }
        case RG_MSG_ERROR: {
            snprintf(msg_text, sizeof(msg_text), TR(RG_TR_RG_ERROR), message);
            priority = 4;
            level = RETRO_LOG_ERROR;
            break;
        }
        case RG_MSG_FOUND: {
            snprintf(msg_text, sizeof(msg_text), TR(RG_TR_RG_FOUND), message);
            duration = 10000;
            priority = 5;
            level = RETRO_LOG_INFO;
            break;
        }
        case RG_MSG_REPLAY_PLAY: {
            snprintf(msg_text, sizeof(msg_text), TR(RG_TR_RG_PLAYBACK),
                rg_main.step_count, rg_bug_glitches.count ? TR(RG_TR_B) : rg_main.localizing ? TR(RG_TR_L) : TR(RG_TR_S));
            duration = 33;
            priority = 6;
            level = RETRO_LOG_INFO;
            type = RETRO_MESSAGE_TYPE_PROGRESS;
            progress = atoi(message);
            break;
        }
        case RG_MSG_REPLAY_REC: {
            snprintf(msg_text, sizeof(msg_text), TR(RG_TR_RG_RECORD));
            duration = 33;
            priority = 6;
            level = RETRO_LOG_INFO;
            type = RETRO_MESSAGE_TYPE_PROGRESS;
            progress = atoi(message);
            break;
        }
        case RG_MSG_DEBUG: {
            return;
            snprintf(msg_text, sizeof(msg_text), TR(RG_TR_RG_DEBUG), message);
            break;
        }
        default:
            break;
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
