#pragma once
#include "settings_migration.h"
// Keep upstream's 13-page migration intact, then append the local IDs:
// 13 = Radio (radio-dev.1), 14 = Power Unit (radio-dev.4). Existing IDs never move.
constexpr size_t LOCAL_TAB_COUNT = 15;
inline size_t radio_migrate_tab_order(uint8_t (&order)[LOCAL_TAB_COUNT], size_t bytes, bool legacy) {
    if (bytes == LOCAL_TAB_COUNT) return bytes;
    if (bytes == 14) { order[14] = 14; return LOCAL_TAB_COUNT; }   // saved with Radio, before Power Unit
    if (bytes < 10 || bytes > 13) return 0;
    uint8_t upstream[13] = {};
    for (size_t i = 0; i < bytes; ++i) upstream[i] = order[i];
    if (rv_migrate_tab_order(upstream, bytes, legacy) != 13) return 0;
    for (size_t i = 0; i < 13; ++i) order[i] = upstream[i];
    order[13] = 13; order[14] = 14;
    return LOCAL_TAB_COUNT;
}
inline size_t radio_migrate_tab_visibility(bool (&enabled)[LOCAL_TAB_COUNT], size_t bytes, bool legacy) {
    if (bytes == LOCAL_TAB_COUNT) return bytes;
    if (bytes == 14) { enabled[14] = true; return LOCAL_TAB_COUNT; }
    if (bytes < 10 || bytes > 13) return 0;
    bool upstream[13] = {};
    for (size_t i = 0; i < bytes; ++i) upstream[i] = enabled[i];
    if (rv_migrate_tab_visibility(upstream, bytes, legacy) != 13) return 0;
    for (size_t i = 0; i < 13; ++i) enabled[i] = upstream[i];
    enabled[13] = true; enabled[14] = true;
    return LOCAL_TAB_COUNT;
}
