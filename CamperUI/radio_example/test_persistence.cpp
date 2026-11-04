#include "../radio_config.h"
#include <Preferences.h>
#include <cassert>
#include <iostream>
int main(){
    radio_config_load();auto initial=radio_endpoint();assert(!*initial.config.host && initial.config.port==80);
    auto c=initial.config;strcpy(c.host,"192.168.8.123");strcpy(c.dab_uris[1],"rtlsdr://dab/5C/RADIO%20BOB%21");
    assert(radio_config_save(c));auto saved=radio_endpoint();assert(saved.generation!=initial.generation);
    radio_config_load();auto loaded=radio_endpoint();assert(!strcmp(loaded.config.host,c.host));assert(!strcmp(loaded.config.dab_uris[1],c.dab_uris[1]));
    auto changed=loaded.config;strcpy(changed.host,"office.local");Preferences::fail_write=true;
    assert(!radio_config_save(changed));assert(radio_endpoint().generation==loaded.generation);
    assert(!strcmp(radio_endpoint().config.host,c.host));Preferences::fail_write=false;
    assert(radio_config_save(changed));assert(!strcmp(radio_endpoint().config.host,"office.local"));
    auto invalid=changed;strcpy(invalid.host,"http://bad");assert(!radio_config_save(invalid));
    // New fields round-trip
    auto power=radio_endpoint().config;strcpy(power.shelly_host,"192.168.8.40");power.battery_guard=25;
    assert(radio_config_save(power));radio_config_load();
    assert(!strcmp(radio_endpoint().config.shelly_host,"192.168.8.40") && radio_endpoint().config.battery_guard==25);
    auto badguard=power;badguard.battery_guard=5;assert(!radio_config_save(badguard));
    badguard.battery_guard=0;strcpy(badguard.shelly_host,"http://x");assert(!radio_config_save(badguard));
    // A damaged current record gives defaults, never older data
    Preferences::store["config_v3"]={1,2,3};radio_config_load();assert(!*radio_endpoint().config.host);
    // Migration from radio-dev.2 (config_v2): settings kept, new fields default
    Preferences::store.clear();
    struct StoredV2 {uint32_t magic;RadioConfigV2 data;} old{};old.magic=0x52414432;
    strcpy(old.data.host,"volumio.local");old.data.port=3000;strcpy(old.data.playlists[0],"A");strcpy(old.data.playlists[1],"B");
    strcpy(old.data.playlists[2],"C");strcpy(old.data.dab_uris[2],"rtlsdr://dab/11D/WDR%202");
    Preferences::store["config_v2"]=std::vector<unsigned char>((unsigned char*)&old,(unsigned char*)&old+sizeof(old));
    radio_config_load();auto m=radio_endpoint().config;
    assert(!strcmp(m.host,"volumio.local") && m.port==3000 && !strcmp(m.playlists[1],"B") && !strcmp(m.dab_uris[2],"rtlsdr://dab/11D/WDR%202"));
    assert(!*m.shelly_host && m.battery_guard==0);
    m.battery_guard=20;assert(radio_config_save(m) && Preferences::store.count("config_v3"));
    radio_config_load();assert(radio_endpoint().config.battery_guard==20 && !strcmp(radio_endpoint().config.host,"volumio.local"));
    std::cout<<"PASS: Shelly/battery guard fields, radio-dev.2 migration\n";
    std::cout<<"PASS: actual radio_config.cpp with simulated NVS: defaults, reload, generation, rejected writes, corrupt record\n";
}
