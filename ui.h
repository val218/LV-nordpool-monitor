// i18n.h — simple three-language string table (EN / LV / RU)
#pragma once

#include <Arduino.h>

enum Lang : uint8_t {
    LANG_EN = 0,
    LANG_LV = 1,
    LANG_RU = 2,
    LANG_COUNT
};

enum StrKey : uint16_t {
    S_TITLE = 0,
    S_CURRENT,
    S_PREV,
    S_NEXT,
    S_CONNECTED,
    S_OFFLINE,
    S_CONFIG_MODE,
    S_RELAYS,
    S_RELAY,
    S_MODE,
    S_OFF,
    S_ON,
    S_MAX_PRICE,
    S_MIN_PRICE,
    S_ALWAYS_OFF,
    S_ALWAYS_ON,
    S_THRESHOLD,
    S_STATE,
    S_LAST_UPDATE,
    S_SETTINGS,
    S_WIFI,
    S_LANGUAGE,
    S_VAT,
    S_SHOW_VAT,
    S_BACK,
    S_SAVE,
    S_SSID,
    S_PASSWORD,
    S_CONNECT,
    S_NO_DATA,
    S_LOADING,
    S_PRICES_TODAY,
    S_PRICES_TOMORROW,
    S_EUR_PER_KWH,
    S_CENTS_PER_KWH,
    S_TURN_ON_BELOW,
    S_TURN_OFF_ABOVE,
    S_NUM_STRINGS
};

// Translation table: [key][lang]
static const char* const TRANSLATIONS[S_NUM_STRINGS][LANG_COUNT] = {
    /* S_TITLE */           { "Nordpool Monitor", "Nordpool Monitors", "Монитор Nordpool" },
    /* S_CURRENT */         { "Current",          "Tagad",             "Сейчас" },
    /* S_PREV */            { "Prev",             "Iepr.",             "Пред." },
    /* S_NEXT */            { "Next",             "Nāk.",              "След." },
    /* S_CONNECTED */       { "Online",           "Tiešsaiste",        "Онлайн" },
    /* S_OFFLINE */         { "Offline",          "Bezsaiste",         "Оффлайн" },
    /* S_CONFIG_MODE */     { "Setup Mode",       "Iestat. rezīms",    "Режим настройки" },
    /* S_RELAYS */          { "Relays",           "Releji",            "Реле" },
    /* S_RELAY */           { "Relay",            "Relejs",            "Реле" },
    /* S_MODE */            { "Mode",             "Režīms",            "Режим" },
    /* S_OFF */             { "OFF",              "IZSL",              "ВЫКЛ" },
    /* S_ON */              { "ON",               "IESL",              "ВКЛ" },
    /* S_MAX_PRICE */       { "Max Price",        "Maks. cena",        "Макс. цена" },
    /* S_MIN_PRICE */       { "Min Price",        "Min. cena",         "Мин. цена" },
    /* S_ALWAYS_OFF */      { "Always OFF",       "Vienmēr IZSL",      "Всегда ВЫКЛ" },
    /* S_ALWAYS_ON */       { "Always ON",        "Vienmēr IESL",      "Всегда ВКЛ" },
    /* S_THRESHOLD */       { "Threshold",        "Slieksnis",         "Порог" },
    /* S_STATE */           { "State",            "Stāvoklis",         "Состояние" },
    /* S_LAST_UPDATE */     { "Updated",          "Atjaun.",           "Обновл." },
    /* S_SETTINGS */        { "Settings",         "Iestatījumi",       "Настройки" },
    /* S_WIFI */            { "Wi-Fi",            "Wi-Fi",             "Wi-Fi" },
    /* S_LANGUAGE */        { "Language",         "Valoda",            "Язык" },
    /* S_VAT */             { "VAT %",            "PVN %",             "НДС %" },
    /* S_SHOW_VAT */        { "Show with VAT",    "Rādīt ar PVN",      "Показать с НДС" },
    /* S_BACK */            { "Back",             "Atpakaļ",           "Назад" },
    /* S_SAVE */            { "Save",             "Saglabāt",          "Сохранить" },
    /* S_SSID */            { "SSID",             "SSID",              "SSID" },
    /* S_PASSWORD */        { "Password",         "Parole",            "Пароль" },
    /* S_CONNECT */         { "Connect",          "Savienot",          "Подключить" },
    /* S_NO_DATA */         { "No data",          "Nav datu",          "Нет данных" },
    /* S_LOADING */         { "Loading...",       "Ielādē...",         "Загрузка..." },
    /* S_PRICES_TODAY */    { "Today",            "Šodien",            "Сегодня" },
    /* S_PRICES_TOMORROW */ { "Tomorrow",         "Rīt",               "Завтра" },
    /* S_EUR_PER_KWH */     { "EUR/kWh",          "EUR/kWh",           "EUR/кВтч" },
    /* S_CENTS_PER_KWH */   { "c/kWh",            "c/kWh",             "ц/кВтч" },
    /* S_TURN_ON_BELOW */   { "ON below",         "IESL zem",          "ВКЛ ниже" },
    /* S_TURN_OFF_ABOVE */  { "OFF above",        "IZSL virs",         "ВЫКЛ выше" },
};

extern Lang g_lang;

inline const char* T(StrKey k) {
    if (k >= S_NUM_STRINGS) return "";
    return TRANSLATIONS[k][g_lang];
}
