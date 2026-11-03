#include "energy_history.h"
#include "energy_history_buffer.h"
#include "system_state.h"
#include <esp_heap_caps.h>
#include <math.h>
#include <new>

namespace {
EnergyHistoryBuffer *history;
uint32_t revision = 1, minute_start, last_read;
bool mode_known, demo_mode;
double sums[3]; unsigned counts[3];
bool incomplete[3];
void clear_accumulator() {
    for (int t=0;t<3;++t) { sums[t]=0; counts[t]=0; incomplete[t]=false; }
}
void finish_minute() {
    EnergySample sample;
    for (int t=0;t<3;++t)
        sample.value[t] = counts[t] && !incomplete[t] ?
            (int16_t)lround(sums[t]/counts[t]) : EnergyHistoryBuffer::missing;
    history->push(sample); ++revision;
    clear_accumulator();
}
bool fresh(uint32_t now, uint32_t stamp) { return now-stamp <= 15000; }
}
void energy_history_init() {
    if (!history) {
        void *memory = heap_caps_malloc(sizeof(EnergyHistoryBuffer), MALLOC_CAP_SPIRAM);
        if (memory) history = new(memory) EnergyHistoryBuffer;
    }
    if (history) history->reset();
    mode_known=false; clear_accumulator(); ++revision;
}
bool energy_history_available() { return history != nullptr; }
bool energy_history_demo() { return demo_mode; }
uint32_t energy_history_version() { return revision; }
void energy_history_tick() {
    if (!history) return;
    uint32_t now=millis();
    float values[3]; bool valid[3], source;
    {
        StateLockGuard lock;
        source=state.debug_mode;
        bool matching=state.data_is_demo == source;
        values[0]=state.bat_soc;
        values[1]=state.bat_voltage*state.bat_current;
        values[2]=state.solar_power;
        bool connected=matching && (source || state.vanpi_connected);
        valid[0]=connected && (state.battery_fields&4) && fresh(now,state.bat_soc_updated_ms);
        valid[1]=connected && (state.battery_fields&3)==3 &&
            fresh(now,state.bat_voltage_updated_ms) && fresh(now,state.bat_current_updated_ms);
        valid[2]=connected && (state.solar_fields&1) && fresh(now,state.solar_power_updated_ms);
    }
    if (!mode_known || source!=demo_mode) {
        history->reset(); clear_accumulator(); ++revision;
        demo_mode=source; mode_known=true; minute_start=now; last_read=now-1000;
    }
    // Mark a pause before committing the minute it may have interrupted.
    if (now-last_read>2000) for(int t=0;t<3;++t) incomplete[t]=true;
    uint32_t elapsed=(now-minute_start)/60000;
    if (elapsed) {
        // Never stretch the previous value across an OTA pause or outage.
        if (elapsed>1) for(int t=0;t<3;++t) incomplete[t]=true;
        finish_minute();
        unsigned gaps=elapsed-1;
        if (gaps>EnergyHistoryBuffer::capacity) gaps=EnergyHistoryBuffer::capacity;
        for(unsigned i=0;i<gaps;++i) history->push({{INT16_MIN,INT16_MIN,INT16_MIN}});
        minute_start += elapsed*60000; ++revision;
    }
    if (now-last_read<1000) return;
    if (now-last_read>2000) for(int t=0;t<3;++t) incomplete[t]=true;
    last_read=now;
    for(int t=0;t<3;++t) {
        bool in_range=isfinite(values[t]) && values[t]>=-32767 && values[t]<=32766 &&
            (t!=0 || (values[t]>=0 && values[t]<=100)) && (t!=2 || values[t]>=0);
        if(valid[t] && in_range) { sums[t]+=values[t]; ++counts[t]; }
        else incomplete[t]=true;
    }
}
bool energy_history_series(int track,int hours,lv_coord_t *points,int count,int *minimum,int *maximum) {
    if (!points || count<1) return false;
    int minutes=hours*60; bool found=false; int low=32767,high=-32768;
    for(int i=0;i<count;++i) points[i]=LV_CHART_POINT_NONE;
    if(!history || track<0 || track>2 || minutes<count || minutes>1440) return false;
    for(int offset=0;offset<minutes;++offset) {
        int16_t v=history->ago(track,offset);
        if(v==EnergyHistoryBuffer::missing) continue;
        found=true; if(v<low)low=v; if(v>high)high=v;
    }
    for(int i=0;i<count;++i) {
        int16_t v=history->bucket(track,minutes,count,i);
        if(v!=EnergyHistoryBuffer::missing)points[i]=v;
    }
    if(minimum)*minimum=found?low:0;
    if(maximum)*maximum=found?high:0;
    return found;
}
