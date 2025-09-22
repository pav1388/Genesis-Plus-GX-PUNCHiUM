// rom_glitcher_translation.c

#include "shared.h"

/* 
    RETRO_LANGUAGE_ENGLISH             = 0,
    RETRO_LANGUAGE_JAPANESE            = 1,
    RETRO_LANGUAGE_FRENCH              = 2,
    RETRO_LANGUAGE_SPANISH             = 3,
    RETRO_LANGUAGE_GERMAN              = 4,
    RETRO_LANGUAGE_ITALIAN             = 5,
    RETRO_LANGUAGE_DUTCH               = 6,
    RETRO_LANGUAGE_PORTUGUESE_BRAZIL   = 7,
    RETRO_LANGUAGE_PORTUGUESE_PORTUGAL = 8,
    RETRO_LANGUAGE_RUSSIAN             = 9,
    RETRO_LANGUAGE_KOREAN              = 10,
    RETRO_LANGUAGE_CHINESE_TRADITIONAL = 11,
    RETRO_LANGUAGE_CHINESE_SIMPLIFIED  = 12,
    RETRO_LANGUAGE_ESPERANTO           = 13,
    RETRO_LANGUAGE_POLISH              = 14,
    RETRO_LANGUAGE_VIETNAMESE          = 15,
    RETRO_LANGUAGE_ARABIC              = 16,
    RETRO_LANGUAGE_GREEK               = 17,
    RETRO_LANGUAGE_TURKISH             = 18,
    RETRO_LANGUAGE_SLOVAK              = 19,
    RETRO_LANGUAGE_PERSIAN             = 20,
    RETRO_LANGUAGE_HEBREW              = 21,
    RETRO_LANGUAGE_ASTURIAN            = 22,
    RETRO_LANGUAGE_FINNISH             = 23,
*/

static enum retro_language current_language = RETRO_LANGUAGE_ENGLISH;
static uint8_t language_packs_size = 1;

static struct {
    enum retro_language language;
    const char* strings[RG_TR_COUNT];
} language_packs[] = {
    {
        .language = RETRO_LANGUAGE_ENGLISH,
        .strings = {
            [RG_TR_LAUNCH_GLITCHER] = "Launch Glitcher " RG_VERSION,
            [RG_TR_LIST_OF_FOUND] = "List of found %u/%u",
            [RG_TR_BRANCH_ALLOWED] = "Branch allowed %u/8",
            [RG_TR_STOP_GLITCHER] = "Stop Glitcher",
            [RG_TR_OPTIONS] = "Options",
            [RG_TR_SEARCH_HEADER] = "[%s]Bug [%s]NOT found [%s]Found [%s]Back",
            [RG_TR_SAVE_STATE] = "Save state",
            [RG_TR_PAUSE_EFFECT] = "Pause effect",
            [RG_TR_PREV_PAGE] = "cur %u/prev %u",
            [RG_TR_NEXT_PAGE] = "next %u",
            [RG_TR_ON] = "ON",
            [RG_TR_OFF] = "off",
            [RG_TR_RG_INFO] = "RG: %s.",
            [RG_TR_RG_ERROR] = "RG ERROR: %s.",
            [RG_TR_RG_FOUND] = "RG FOUND: %s.",
            [RG_TR_RG_PLAYBACK] = "RG PLAYBACK: 'Menu' to stop. Step %d [%s]",
            [RG_TR_RG_RECORD] = "RG RECORD: 'Menu' to finish.",
            [RG_TR_RG_DEBUG] = "RG DEBUG: %s.",
            [RG_TR_B] = "B", // bug
            [RG_TR_S] = "S", // search
            [RG_TR_L] = "L", // localizing
            [RG_TR_BUG] = "BUG",
            [RG_TR_SEARCH] = "SEARCH",
            [RG_TR_LOCAL] = "LOCAL",
            [RG_TR_STEP] = "Step",
            [RG_TR_CANDIDATES] = "Candidates",
            [RG_TR_FOUND] = "found",
            [RG_TR_RANGE] = "Range",
            [RG_TR_GLITCH] = "Glitch",
            [RG_TR_EMPTY_SLOT] = "Empty slot",
            [RG_TR_CONTINUE_SEARCH] = "Continue searching ...",
            [RG_TR_ADDRESS_CHANGED] = "Selected addresses have been changed",
            [RG_TR_CHECKSUM_CORRECTED] = "Checksum in header has been corrected",
            [RG_TR_DUPLICATE] = "Duplicate. Address is already saved in cht-file",
            [RG_TR_GLITCH_SAVED_TO] = "Glitch saved to",
            [RG_TR_ERROR_COMMON_1] = "Candidates not found",
            [RG_TR_ERROR_COMMON_2] = "Slots are full. First glitch removed",
            [RG_TR_ERROR_COMMON_3] = "No last game info",
            [RG_TR_ERROR_COMMON_4] = "State not saved",
            [RG_TR_ERROR_COMMON_5] = "State not loaded",
            [RG_TR_ERROR_COMMON_6] = "Over 65,000 bug-steps. Start new search",
            [RG_TR_ERROR_MEMORY] = "Allocating memory",
            [RG_TR_ERROR_BACKUP] = "No backup data",
            [RG_TR_ERROR_ADDRESS] = "Address not found. Try again",
            [RG_TR_EMPTY] = NULL
        }
    },
    {
        .language = RETRO_LANGUAGE_RUSSIAN,
        .strings = {
            [RG_TR_LAUNCH_GLITCHER] = "Зaпуcк Glitcher " RG_VERSION,
            [RG_TR_LIST_OF_FOUND] = "Нaйдeнныe %u/%u",
            [RG_TR_BRANCH_ALLOWED] = "Ветвления %u/8",
            [RG_TR_STOP_GLITCHER] = "Ocтaнoвкa Glitcher",
            [RG_TR_OPTIONS] = "Oпции",
            [RG_TR_SEARCH_HEADER] = "[%s]Бaг [%s]Нe нaйдeн [%s]Нaйдeн [%s]Нaзaд",
            [RG_TR_SAVE_STATE] = "Сохранить состояние",
            [RG_TR_PAUSE_EFFECT] = "Эффект при паузе",
            [RG_TR_PREV_PAGE] = "тeк %u/прeд %u",
            [RG_TR_NEXT_PAGE] = "cлeд %u",
            [RG_TR_ON] = "ON",
            [RG_TR_OFF] = "off",
            [RG_TR_RG_INFO] = "RG: %s.",
            [RG_TR_RG_ERROR] = "RG ОШИБКА: %s.",
            [RG_TR_RG_FOUND] = "RG НАЙДЕНО: %s.",
            [RG_TR_RG_PLAYBACK] = "RG ВОСПРОИЗВЕДЕНИЕ: 'Меню' для остановки. Шаг %d [%s]",
            [RG_TR_RG_RECORD] = "RG ЗАПИСЬ: 'Меню' для завершения.",
            [RG_TR_RG_DEBUG] = "RG ОТЛАДКА: %s.",
            [RG_TR_B] = "Б", // баг
            [RG_TR_S] = "П", // поиск
            [RG_TR_L] = "Л", // локализация
            [RG_TR_BUG] = "БАГ",
            [RG_TR_SEARCH] = "ПОИСК",
            [RG_TR_LOCAL] = "ЛОКАЛ",
            [RG_TR_STEP] = "Шаг",
            [RG_TR_CANDIDATES] = "Кандидаты",
            [RG_TR_FOUND] = "найдено",
            [RG_TR_RANGE] = "Диапазон",
            [RG_TR_GLITCH] = "Глитч",
            [RG_TR_EMPTY_SLOT] = "Пустой слот",
            [RG_TR_CONTINUE_SEARCH] = "Продолжение поиска ...",
            [RG_TR_ADDRESS_CHANGED] = "Выбранные адреса были изменены",
            [RG_TR_CHECKSUM_CORRECTED] = "Контрольная сумма в заголовке исправлена",
            [RG_TR_DUPLICATE] = "Дубликат. Адрес уже сохранена в cht-файле",
            [RG_TR_GLITCH_SAVED_TO] = "Глитч сохранён в",
            [RG_TR_ERROR_COMMON_1] = "Кандидаты не найдены",
            [RG_TR_ERROR_COMMON_2] = "Все слоты заняты. Первый глитч удалён",
            [RG_TR_ERROR_COMMON_3] = "Нет информации о последней игре",
            [RG_TR_ERROR_COMMON_4] = "Состояние не сохранено",
            [RG_TR_ERROR_COMMON_5] = "Состояние не загружено",
            [RG_TR_ERROR_COMMON_6] = "Более 65,000 баг-шагов. Начните новый поиск",
            [RG_TR_ERROR_MEMORY] = "Выделение памяти",
            [RG_TR_ERROR_BACKUP] = "Нет данных резервной копии",
            [RG_TR_ERROR_ADDRESS] = "Адрес не найден. Попробуйте ещё раз",
            [RG_TR_EMPTY] = NULL
        }
    }
};

void rg_get_language(void) {
    if (environ_cb)
        environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &current_language);

    language_packs_size = ARRAY_SIZE(language_packs);
}

const char* rg_get_translation(rom_glitcher_translation_t id) {
    if (id < 0 || id >= RG_TR_COUNT)
        return "!eRRoR string!";

    for (uint8_t i = 0; i < language_packs_size; i++)
        if (language_packs[i].language == current_language && language_packs[i].strings[id] != NULL)
            return language_packs[i].strings[id];

    for (uint8_t i = 0; i < language_packs_size; i++)
        if (language_packs[i].language == RETRO_LANGUAGE_ENGLISH && language_packs[i].strings[id] != NULL)
            return language_packs[i].strings[id];

    return "!MiSSiNG string!";
}
