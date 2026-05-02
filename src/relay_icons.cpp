// relay_icons.cpp — icon catalog and lookup helpers.
#include "relay_icons.h"

// FontAwesome 5 Free Solid codepoints used by the icon font generator.
// If you add or change icons here, also update tools/build_icons.sh so the
// font is regenerated with the right glyphs included.
//
// Codepoints (FA5 Solid):
//   plug                f1e6
//   lightbulb           f0eb
//   fire                f06d   (heater)
//   shower              f2cc   (water heater)
//   snowflake           f2dc   (AC)
//   fan                 f863
//   ice-cream / temp    f2cb   (fridge — using temperature-low)
//   washer              tshirt f553 as proxy
//   wind                f72e   (dryer proxy)
//   utensils            f2e7   (dishwasher proxy)
//   bread-slice         f7ec   (oven proxy)
//   square (microwave)  f0c8   (placeholder)
//   coffee              f0f4   (kettle proxy)
//   tshirt-iron         f553   (iron — same as washer for now)
//   bolt                f0e7   (charger / EV)
//   tint                f043   (pump — water drop)
//   thermometer-half    f2c9   (thermostat)
//   tv                  f26c
//   desktop             f108   (computer)
//   sun                 f185   (lighting)
//   circle              f111   (generic / fallback)
const IconCatalogEntry g_iconCatalog[ICON_COUNT] = {
    /* ICON_GENERIC */     { "\xEF\x84\x91", "[ ]",  "Generic" },
    /* ICON_SOCKET */      { "\xEF\x87\xA6", "PWR",  "Socket" },
    /* ICON_BULB */        { "\xEF\x83\xAB", "BLB",  "Light bulb" },
    /* ICON_HEATER */      { "\xEF\x81\xAD", "HTR",  "Heater" },
    /* ICON_WATER_HEATER */{ "\xEF\x8B\x8C", "H2O",  "Water heater" },
    /* ICON_AC */          { "\xEF\x8B\x9C", "AC",   "Air conditioner" },
    /* ICON_FAN */         { "\xEF\xA1\xA3", "FAN",  "Fan" },
    /* ICON_FRIDGE */      { "\xEF\x8B\x8B", "FRG",  "Fridge" },
    /* ICON_WASHER */      { "\xEF\x95\x93", "WSH",  "Washing machine" },
    /* ICON_DRYER */       { "\xEF\x9C\xAE", "DRY",  "Dryer" },
    /* ICON_DISHWASHER */  { "\xEF\x8B\xA7", "DSH",  "Dishwasher" },
    /* ICON_OVEN */        { "\xEF\x9F\xAC", "OVN",  "Oven" },
    /* ICON_MICROWAVE */   { "\xEF\x83\x88", "MWV",  "Microwave" },
    /* ICON_KETTLE */      { "\xEF\x83\xB4", "KTL",  "Kettle" },
    /* ICON_IRON */        { "\xEF\x95\x93", "IRN",  "Iron" },
    /* ICON_CHARGER */     { "\xEF\x83\xA7", "EV",   "Charger / EV" },
    /* ICON_PUMP */        { "\xEF\x81\x83", "PMP",  "Pump" },
    /* ICON_THERMOSTAT */  { "\xEF\x8B\x89", "THR",  "Thermostat" },
    /* ICON_TV */          { "\xEF\x89\xAC", "TV",   "Television" },
    /* ICON_COMPUTER */    { "\xEF\x84\x88", "PC",   "Computer" },
    /* ICON_LIGHTING */    { "\xEF\x86\x85", "LGT",  "Lighting" },
};

const char* iconText(RelayIcon i) {
    if ((uint8_t)i >= ICON_COUNT) i = ICON_GENERIC;
#ifdef NP_ICONS_AVAILABLE
    return g_iconCatalog[i].utf8;
#else
    return g_iconCatalog[i].fallback;
#endif
}

const char* iconLabel(RelayIcon i) {
    if ((uint8_t)i >= ICON_COUNT) i = ICON_GENERIC;
    return g_iconCatalog[i].label;
}
