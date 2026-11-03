#pragma once
#include <stdint.h>
#include <stddef.h>
// IDs 0..10 match upstream V1.6.1. Old RV3D.90 used 10 for Victron.
template<size_t N> constexpr size_t rv_migrate_tab_order(uint8_t (&order)[N],size_t bytes,bool legacy_rv11) {
    static_assert(N>=12,"Truma, Victron, and Fridge require stable page IDs");
    if(bytes==10){order[10]=10;order[11]=11;if (N>12) order[12]=12;return N;}
    if(bytes==11) {
        if(legacy_rv11){for(size_t i=0;i<11;++i)if(order[i]==10)order[i]=11;order[11]=10;}
        else order[11]=11;
        if (N>12) order[12]=12;
        return N;
    }
    if(bytes==12 && N>12) {
        order[12]=12;
        return N;
    }
    return bytes;
}
template<size_t N> constexpr size_t rv_migrate_tab_visibility(bool (&enabled)[N],size_t bytes,bool legacy_rv11) {
    static_assert(N>=12,"Truma, Victron, and Fridge require stable page IDs");
    if(bytes==10){enabled[10]=enabled[11]=false;if (N>12) enabled[12]=false;return N;}
    if(bytes==11) {
        if(legacy_rv11){enabled[11]=enabled[10];enabled[10]=false;}
        else enabled[11]=false;
        if (N>12) enabled[12]=false;
        return N;
    }
    if(bytes==12 && N>12) {
        enabled[12]=false;
        return N;
    }
    return bytes;
}
constexpr int rv_migrate_heater_type(int value,bool legacy_enum) {
    // Old NONE=2 must not become the newly introduced Truma+Electric=2.
    return legacy_enum&&value==2?3:value;
}
