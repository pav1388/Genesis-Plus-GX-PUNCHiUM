// rom_glitcher_translation.h

#ifndef _ROM_GLITCHER_TRANSLATION_H_
#define _ROM_GLITCHER_TRANSLATION_H_

#define TR(id) rg_get_translation(id)

typedef enum {
    RG_TR_LAUNCH_GLITCHER,
    RG_TR_LIST_OF_FOUND,
    RG_TR_BRANCH_ALLOWED,
    RG_TR_STOP_GLITCHER,
    RG_TR_OPTIONS,
    RG_TR_SEARCH_HEADER,
    RG_TR_SAVE_STATE,
    RG_TR_PAUSE_EFFECT,
    RG_TR_PREV_PAGE,
    RG_TR_NEXT_PAGE,
    RG_TR_ON,
    RG_TR_OFF,
    RG_TR_RG_INFO,
    RG_TR_RG_ERROR,
    RG_TR_RG_FOUND,
    RG_TR_RG_PLAYBACK,
    RG_TR_RG_RECORD,
    RG_TR_RG_DEBUG,
    RG_TR_B,
    RG_TR_S,
    RG_TR_L,
    RG_TR_BUG,
    RG_TR_SEARCH,
    RG_TR_LOCAL,
    RG_TR_STEP,
    RG_TR_CANDIDATES,
    RG_TR_FOUND,
    RG_TR_RANGE, 
    RG_TR_GLITCH,
    RG_TR_CONTINUE_SEARCH,
    RG_TR_ADDRESS_CHANGED,
    RG_TR_CHECKSUM_CORRECTED, // not used
    RG_TR_DUPLICATE,
    RG_TR_GLITCH_SAVED_TO,
    RG_TR_ERROR_COMMON_1, 
    RG_TR_ERROR_COMMON_2, 
    RG_TR_ERROR_COMMON_3, 
    RG_TR_ERROR_COMMON_4, 
    RG_TR_ERROR_COMMON_5, 
    RG_TR_ERROR_COMMON_6, 
    RG_TR_ERROR_MEMORY,
    RG_TR_ERROR_BACKUP,
    RG_TR_ERROR_ADDRESS,
    RG_TR_ERROR_FILE_ACCESS,
    RG_TR_EMPTY,
    RG_TR_COUNT
} rom_glitcher_translation_t;

void rg_get_language(void);
const char* rg_get_translation(rom_glitcher_translation_t id);

#endif