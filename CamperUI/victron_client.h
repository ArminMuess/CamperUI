#pragma once
#include <Arduino.h>
#include "victron_protocol.h"
enum GxField:uint8_t {
    GX_MODE,GX_ADJUSTABLE,GX_DEVICE_CONNECTED,GX_STATE,
    GX_IN_V,GX_IN_I,GX_IN_P,GX_IN_TOTAL,GX_OUT_V,GX_OUT_I,GX_OUT_P,GX_OUT_TOTAL,
    GX_IN_F,GX_OUT_F,GX_DC_V,GX_DC_I,GX_SOC,GX_PV,
    GX_RELAY0,GX_RELAY1,GX_FUNCTION0,GX_FUNCTION1,GX_STARTER,GX_STARTER_CONNECTED,GX_FIELD_COUNT
};
struct GxValue { float value=0;uint32_t at=0;bool valid=false,live=false; };
struct VictronSnapshot {
    bool enabled=false,demo=false,connected=false,stale=true,ambiguous=false;
    char portal[33]={},product[64]={},message[112]={};
    int instance=-1,system_instance=-1,settings_instance=0;
    int battery_instance=-1;bool starter_ambiguous=false;
    uint32_t session=1,last_message=0;
    GxValue values[GX_FIELD_COUNT];
    GxPending pending[3]; // Mode, relay 1, relay 2
};
void victron_init();
void victron_tick();
void victron_settings_changed();
VictronSnapshot victron_snapshot();
bool victron_fresh(const VictronSnapshot&,GxField);
bool victron_mode_available(const VictronSnapshot&);
bool victron_relay_available(const VictronSnapshot&,int);
bool victron_request_mode(int);
bool victron_request_relay(int,bool);
const char *victron_mode_name(int);
const char *victron_state_name(int);
const char *victron_status(const VictronSnapshot&);
