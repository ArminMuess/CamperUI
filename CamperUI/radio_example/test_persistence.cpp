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
    Preferences::store["config_v2"]={1,2,3};radio_config_load();assert(!*radio_endpoint().config.host);
    std::cout<<"PASS: actual radio_config.cpp with simulated NVS: defaults, reload, generation, rejected writes, corrupt record\n";
}
