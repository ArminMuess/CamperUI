#pragma once
#include "settings_migration.h"
// Keep upstream's 13-page migration intact, then append the local Radio ID.
inline size_t radio_migrate_tab_order(uint8_t (&order)[14], size_t bytes, bool legacy) {
    if (bytes == 14) return bytes;
    if (bytes < 10 || bytes > 13) return 0;
    uint8_t upstream[13] = {};
    for (size_t i = 0; i < bytes; ++i) upstream[i] = order[i];
    if (rv_migrate_tab_order(upstream, bytes, legacy) != 13) return 0;
    for (size_t i = 0; i < 13; ++i) order[i] = upstream[i];
    order[13] = 13;
    return 14;
}
inline size_t radio_migrate_tab_visibility(bool (&enabled)[14], size_t bytes, bool legacy) {
    if (bytes == 14) return bytes;
    if (bytes < 10 || bytes > 13) return 0;
    bool upstream[13] = {};
    for (size_t i = 0; i < bytes; ++i) upstream[i] = enabled[i];
    if (rv_migrate_tab_visibility(upstream, bytes, legacy) != 13) return 0;
    for (size_t i = 0; i < 13; ++i) enabled[i] = upstream[i];
    enabled[13] = true;
    return 14;
}
