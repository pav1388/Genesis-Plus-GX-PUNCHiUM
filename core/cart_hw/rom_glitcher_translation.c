// rom_glitcher_translation.c

//#include "shared.h"
#include "rom_glitcher_translation.h"
#include "shared.h"
#include <stdint.h>

/* 
    RETRO_LANGUAGE_ENGLISH             = 0,     +
    RETRO_LANGUAGE_JAPANESE            = 1,
    RETRO_LANGUAGE_FRENCH              = 2,
    RETRO_LANGUAGE_SPANISH             = 3,     +
    RETRO_LANGUAGE_GERMAN              = 4,     +
    RETRO_LANGUAGE_ITALIAN             = 5,     +
    RETRO_LANGUAGE_DUTCH               = 6,
    RETRO_LANGUAGE_PORTUGUESE_BRAZIL   = 7,     +
    RETRO_LANGUAGE_PORTUGUESE_PORTUGAL = 8,
    RETRO_LANGUAGE_RUSSIAN             = 9,     +
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

    ENGLISH RUSSIAN SPANISH PORTUGUESE_BRAZIL GERMAN ITALIAN
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
            [RG_TR_LIST_OF_FOUND] = "List of found",
            [RG_TR_INSTRUCTION_FILTER] = "Instruction Filter",
            [RG_TR_INSTRUCTION_ARITHMETIC] = "Arithmetic '+-'",
            [RG_TR_INSTRUCTION_BRANCHING] = "Branching",
            [RG_TR_INSTRUCTION_SETTING] = "Setting",
            [RG_TR_INSTRUCTION_LOOPING] = "Looping",
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
            [RG_TR_ERROR_FILE_ACCESS] = "File access denied",
            [RG_TR_EMPTY] = NULL
        }
    },
    {
        .language = RETRO_LANGUAGE_RUSSIAN,
        .strings = {
            [RG_TR_LAUNCH_GLITCHER] = "Зaпуcк Glitcher " RG_VERSION,
            [RG_TR_LIST_OF_FOUND] = "Нaйдeнныe",
            [RG_TR_INSTRUCTION_FILTER] = "Фильтp инcтрукций",
            [RG_TR_INSTRUCTION_ARITHMETIC] = "Аpифмeтика '+-'",
            [RG_TR_INSTRUCTION_BRANCHING] = "Вeтвлeния",
            [RG_TR_INSTRUCTION_SETTING] = "Уcтaнoвкa",
            [RG_TR_INSTRUCTION_LOOPING] = "Циклы",
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
            [RG_TR_FOUND] = "в найденном",
            [RG_TR_RANGE] = "Диапазон",
            [RG_TR_GLITCH] = "Глитч",
            [RG_TR_CONTINUE_SEARCH] = "Продолжение поиска ...",
            [RG_TR_ADDRESS_CHANGED] = "Выбранные адреса были изменены",
            [RG_TR_CHECKSUM_CORRECTED] = "Контрольная сумма в заголовке исправлена",
            [RG_TR_DUPLICATE] = "Дубликат. Адрес уже сохранён в cht-файле",
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
            [RG_TR_ERROR_FILE_ACCESS] = "Доступ к файлу запрещен",
            [RG_TR_EMPTY] = NULL
        }
    },
    {
        .language = RETRO_LANGUAGE_SPANISH,
        .strings = {
            [RG_TR_LAUNCH_GLITCHER] = "Lanzar Glitcher " RG_VERSION,
            [RG_TR_LIST_OF_FOUND] = "Lista de encontrados",
            [RG_TR_INSTRUCTION_FILTER] = "Filtro de instrucciones",
            [RG_TR_INSTRUCTION_ARITHMETIC] = "Aritmética '+-'",
            [RG_TR_INSTRUCTION_BRANCHING] = "Bifurcación",
            [RG_TR_INSTRUCTION_SETTING] = "Configuración",
            [RG_TR_INSTRUCTION_LOOPING] = "Bucleo",
            [RG_TR_STOP_GLITCHER] = "Detener Glitcher",
            [RG_TR_OPTIONS] = "Opciones",
            [RG_TR_SEARCH_HEADER] = "[%s]Error [%s]NO encontrado [%s]Encontrado [%s]Atrás",
            [RG_TR_SAVE_STATE] = "Guardar estado",
            [RG_TR_PAUSE_EFFECT] = "Efecto en pausa",
            [RG_TR_PREV_PAGE] = "act %u/ant %u",
            [RG_TR_NEXT_PAGE] = "sig %u",
            [RG_TR_ON] = "ON",
            [RG_TR_OFF] = "off",
            [RG_TR_RG_INFO] = "RG: %s.",
            [RG_TR_RG_ERROR] = "RG ERROR: %s.",
            [RG_TR_RG_FOUND] = "RG ENCONTRADO: %s.",
            [RG_TR_RG_PLAYBACK] = "RG REPRODUCIENDO: 'Menú' para parar. Paso %d [%s]",
            [RG_TR_RG_RECORD] = "RG GRABANDO: 'Menú' para finalizar.",
            [RG_TR_RG_DEBUG] = "RG DEPURACIÓN: %s.",
            [RG_TR_B] = "E", // error (bug)
            [RG_TR_S] = "B", // búsqueda
            [RG_TR_L] = "L", // localización
            [RG_TR_BUG] = "ERROR",
            [RG_TR_SEARCH] = "BUSCAR",
            [RG_TR_LOCAL] = "LOCAL",
            [RG_TR_STEP] = "Paso",
            [RG_TR_CANDIDATES] = "Candidatos",
            [RG_TR_FOUND] = "encontrado",
            [RG_TR_RANGE] = "Rango",
            [RG_TR_GLITCH] = "Glitch",
            [RG_TR_CONTINUE_SEARCH] = "Continuar búsqueda ...",
            [RG_TR_ADDRESS_CHANGED] = "Las direcciones seleccionadas han cambiado",
            [RG_TR_CHECKSUM_CORRECTED] = "Checksum en cabecera ha sido corregida",
            [RG_TR_DUPLICATE] = "Duplicado. La dirección ya está guardada en cht-file",
            [RG_TR_GLITCH_SAVED_TO] = "Glitch guardado en",
            [RG_TR_ERROR_COMMON_1] = "Candidatos no encontrados",
            [RG_TR_ERROR_COMMON_2] = "Ranuras llenas. Primer glitch eliminado",
            [RG_TR_ERROR_COMMON_3] = "No hay info del último juego",
            [RG_TR_ERROR_COMMON_4] = "Estado no guardado",
            [RG_TR_ERROR_COMMON_5] = "Estado no cargado",
            [RG_TR_ERROR_COMMON_6] = "Más de 65,000 pasos-error. Comience nueva búsqueda",
            [RG_TR_ERROR_MEMORY] = "Asignando memoria",
            [RG_TR_ERROR_BACKUP] = "No hay datos de respaldo",
            [RG_TR_ERROR_ADDRESS] = "Dirección no encontrada. Intente de nuevo",
            [RG_TR_ERROR_FILE_ACCESS] = "Acceso al archivo denegado",
            [RG_TR_EMPTY] = NULL
        }
    },
    {
        .language = RETRO_LANGUAGE_PORTUGUESE_BRAZIL,
        .strings = {
            [RG_TR_LAUNCH_GLITCHER] = "Iniciar Glitcher " RG_VERSION,
            [RG_TR_LIST_OF_FOUND] = "Lista de encontrados",
            [RG_TR_INSTRUCTION_FILTER] = "Filtro de instruções",
            [RG_TR_INSTRUCTION_ARITHMETIC] = "Aritmética '+-'",
            [RG_TR_INSTRUCTION_BRANCHING] = "Ramificação",
            [RG_TR_INSTRUCTION_SETTING] = "Definição",
            [RG_TR_INSTRUCTION_LOOPING] = "Looping",
            [RG_TR_STOP_GLITCHER] = "Parar Glitcher",
            [RG_TR_OPTIONS] = "Opções",
            [RG_TR_SEARCH_HEADER] = "[%s]Bug [%s]NÃO encontrado [%s]Encontrado [%s]Voltar",
            [RG_TR_SAVE_STATE] = "Salvar estado",
            [RG_TR_PAUSE_EFFECT] = "Efeito no pause",
            [RG_TR_PREV_PAGE] = "atual %u/ant %u",
            [RG_TR_NEXT_PAGE] = "próx %u",
            [RG_TR_ON] = "ON",
            [RG_TR_OFF] = "off",
            [RG_TR_RG_INFO] = "RG: %s.",
            [RG_TR_RG_ERROR] = "RG ERRO: %s.",
            [RG_TR_RG_FOUND] = "RG ENCONTRADO: %s.",
            [RG_TR_RG_PLAYBACK] = "RG REPRODUZINDO: 'Menu' para parar. Passo %d [%s]",
            [RG_TR_RG_RECORD] = "RG GRAVANDO: 'Menu' para finalizar.",
            [RG_TR_RG_DEBUG] = "RG DEPURAÇÃO: %s.",
            [RG_TR_B] = "B", // bug
            [RG_TR_S] = "P", // pesquisa
            [RG_TR_L] = "L", // localização
            [RG_TR_BUG] = "BUG",
            [RG_TR_SEARCH] = "PESQUISAR",
            [RG_TR_LOCAL] = "LOCAL",
            [RG_TR_STEP] = "Passo",
            [RG_TR_CANDIDATES] = "Candidatos",
            [RG_TR_FOUND] = "encontrado",
            [RG_TR_RANGE] = "Intervalo",
            [RG_TR_GLITCH] = "Glitch",
            [RG_TR_CONTINUE_SEARCH] = "Continuar pesquisa ...",
            [RG_TR_ADDRESS_CHANGED] = "Endereços selecionados foram alterados",
            [RG_TR_CHECKSUM_CORRECTED] = "Checksum no cabeçalho foi corrigida",
            [RG_TR_DUPLICATE] = "Duplicado. Endereço já salvo no cht-file",
            [RG_TR_GLITCH_SAVED_TO] = "Glitch salvo em",
            [RG_TR_ERROR_COMMON_1] = "Candidatos não encontrados",
            [RG_TR_ERROR_COMMON_2] = "Slots cheios. Primeiro glitch removido",
            [RG_TR_ERROR_COMMON_3] = "Sem info do último jogo",
            [RG_TR_ERROR_COMMON_4] = "Estado não salvo",
            [RG_TR_ERROR_COMMON_5] = "Estado não carregado",
            [RG_TR_ERROR_COMMON_6] = "Mais de 65,000 passos-bug. Comece nova pesquisa",
            [RG_TR_ERROR_MEMORY] = "Alocando memória",
            [RG_TR_ERROR_BACKUP] = "Sem dados de backup",
            [RG_TR_ERROR_ADDRESS] = "Endereço não encontrado. Tente novamente",
            [RG_TR_ERROR_FILE_ACCESS] = "Acesso ao arquivo negado",
            [RG_TR_EMPTY] = NULL
        }
    },
    {
        .language = RETRO_LANGUAGE_GERMAN,
        .strings = {
            [RG_TR_LAUNCH_GLITCHER] = "Starte Glitcher " RG_VERSION,
            [RG_TR_LIST_OF_FOUND] = "Liste der Funde",
            [RG_TR_INSTRUCTION_FILTER] = "Befehlsfilter",
            [RG_TR_INSTRUCTION_ARITHMETIC] = "Arithmetik '+-'",
            [RG_TR_INSTRUCTION_BRANCHING] = "Verzweigung",
            [RG_TR_INSTRUCTION_SETTING] = "Setzen",
            [RG_TR_INSTRUCTION_LOOPING] = "Schleifen",
            [RG_TR_STOP_GLITCHER] = "Stoppe Glitcher",
            [RG_TR_OPTIONS] = "Optionen",
            [RG_TR_SEARCH_HEADER] = "[%s]Fehler [%s]NICHT gefunden [%s]Gefunden [%s]Zurück",
            [RG_TR_SAVE_STATE] = "Status speichern",
            [RG_TR_PAUSE_EFFECT] = "Pauseneffekt",
            [RG_TR_PREV_PAGE] = "akt %u/vorh %u",
            [RG_TR_NEXT_PAGE] = "näch %u",
            [RG_TR_ON] = "ON",
            [RG_TR_OFF] = "off",
            [RG_TR_RG_INFO] = "RG: %s.",
            [RG_TR_RG_ERROR] = "RG FEHLER: %s.",
            [RG_TR_RG_FOUND] = "RG GEFUNDEN: %s.",
            [RG_TR_RG_PLAYBACK] = "RG WIEDERGABE: 'Menü' zum Stoppen. Schritt %d [%s]",
            [RG_TR_RG_RECORD] = "RG AUFNAHME: 'Menü' zum Beenden.",
            [RG_TR_RG_DEBUG] = "RG DEBUG: %s.",
            [RG_TR_B] = "F", // Fehler
            [RG_TR_S] = "S", // Suche
            [RG_TR_L] = "L", // Lokalisierung
            [RG_TR_BUG] = "FEHLER",
            [RG_TR_SEARCH] = "SUCHE",
            [RG_TR_LOCAL] = "LOKAL",
            [RG_TR_STEP] = "Schritt",
            [RG_TR_CANDIDATES] = "Kandidaten",
            [RG_TR_FOUND] = "gefunden",
            [RG_TR_RANGE] = "Bereich",
            [RG_TR_GLITCH] = "Glitch",
            [RG_TR_CONTINUE_SEARCH] = "Suche wird fortgesetzt ...",
            [RG_TR_ADDRESS_CHANGED] = "Ausgewählte Adressen wurden geändert",
            [RG_TR_CHECKSUM_CORRECTED] = "Prüfsumme im Header wurde korrigiert",
            [RG_TR_DUPLICATE] = "Duplikat. Adresse bereits in cht-file gespeichert",
            [RG_TR_GLITCH_SAVED_TO] = "Glitch gespeichert in",
            [RG_TR_ERROR_COMMON_1] = "Kandidaten nicht gefunden",
            [RG_TR_ERROR_COMMON_2] = "Slots voll. Erster Glitch entfernt",
            [RG_TR_ERROR_COMMON_3] = "Keine letzten Spielinfos",
            [RG_TR_ERROR_COMMON_4] = "Status nicht gespeichert",
            [RG_TR_ERROR_COMMON_5] = "Status nicht geladen",
            [RG_TR_ERROR_COMMON_6] = "Über 65,000 Fehlerschritte. Neue Suche starten",
            [RG_TR_ERROR_MEMORY] = "Speicherzuweisung",
            [RG_TR_ERROR_BACKUP] = "Keine Backup-Daten",
            [RG_TR_ERROR_ADDRESS] = "Adresse nicht gefunden. Versuchen Sie es erneut",
            [RG_TR_ERROR_FILE_ACCESS] = "Dateizugriff verweigert",
            [RG_TR_EMPTY] = NULL
        }
    },
    {
        .language = RETRO_LANGUAGE_ITALIAN,
        .strings = {
            [RG_TR_LAUNCH_GLITCHER] = "Avvia Glitcher " RG_VERSION,
            [RG_TR_LIST_OF_FOUND] = "Lista dei trovati",
            [RG_TR_INSTRUCTION_FILTER] = "Filtro istruzioni",
            [RG_TR_INSTRUCTION_ARITHMETIC] = "Aritmetica '+-'",
            [RG_TR_INSTRUCTION_BRANCHING] = "Branching",
            [RG_TR_INSTRUCTION_SETTING] = "Impostazione",
            [RG_TR_INSTRUCTION_LOOPING] = "Cicli",
            [RG_TR_STOP_GLITCHER] = "Ferma Glitcher",
            [RG_TR_OPTIONS] = "Opzioni",
            [RG_TR_SEARCH_HEADER] = "[%s]Bug [%s]NON trovato [%s]Trovato [%s]Indietro",
            [RG_TR_SAVE_STATE] = "Salva stato",
            [RG_TR_PAUSE_EFFECT] = "Effetto in pausa",
            [RG_TR_PREV_PAGE] = "att %u/prec %u",
            [RG_TR_NEXT_PAGE] = "succ %u",
            [RG_TR_ON] = "ON",
            [RG_TR_OFF] = "off",
            [RG_TR_RG_INFO] = "RG: %s.",
            [RG_TR_RG_ERROR] = "RG ERRORE: %s.",
            [RG_TR_RG_FOUND] = "RG TROVATO: %s.",
            [RG_TR_RG_PLAYBACK] = "RG RIPRODUZIONE: 'Menu' per fermare. Passo %d [%s]",
            [RG_TR_RG_RECORD] = "RG REGISTRAZIONE: 'Menu' per finire.",
            [RG_TR_RG_DEBUG] = "RG DEBUG: %s.",
            [RG_TR_B] = "B", // bug
            [RG_TR_S] = "C", // cerca
            [RG_TR_L] = "L", // localizzazione
            [RG_TR_BUG] = "BUG",
            [RG_TR_SEARCH] = "CERCA",
            [RG_TR_LOCAL] = "LOCALE",
            [RG_TR_STEP] = "Passo",
            [RG_TR_CANDIDATES] = "Candidati",
            [RG_TR_FOUND] = "trovato",
            [RG_TR_RANGE] = "Intervallo",
            [RG_TR_GLITCH] = "Glitch",
            [RG_TR_CONTINUE_SEARCH] = "Continua ricerca ...",
            [RG_TR_ADDRESS_CHANGED] = "Indirizzi selezionati sono stati cambiati",
            [RG_TR_CHECKSUM_CORRECTED] = "Checksum nell'header è stata corretta",
            [RG_TR_DUPLICATE] = "Duplicato. Indirizzo già salvato in cht-file",
            [RG_TR_GLITCH_SAVED_TO] = "Glitch salvato in",
            [RG_TR_ERROR_COMMON_1] = "Candidati non trovati",
            [RG_TR_ERROR_COMMON_2] = "Slot pieni. Primo glitch rimosso",
            [RG_TR_ERROR_COMMON_3] = "Nessuna info ultimo gioco",
            [RG_TR_ERROR_COMMON_4] = "Stato non salvato",
            [RG_TR_ERROR_COMMON_5] = "Stato non caricato",
            [RG_TR_ERROR_COMMON_6] = "Oltre 65,000 passi-bug. Inizia nuova ricerca",
            [RG_TR_ERROR_MEMORY] = "Allocazione memoria",
            [RG_TR_ERROR_BACKUP] = "Nessun dato di backup",
            [RG_TR_ERROR_ADDRESS] = "Indirizzo non trovato. Riprova",
            [RG_TR_ERROR_FILE_ACCESS] = "Accesso al file negato",
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
