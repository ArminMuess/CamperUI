#include "radio_config.h"
#include <Arduino.h>
#include <Preferences.h>
namespace {
portMUX_TYPE config_guard=portMUX_INITIALIZER_UNLOCKED;
RadioConfig config;
uint32_t generation=1;
}
RadioEndpoint radio_endpoint() {
    RadioEndpoint e;
    portENTER_CRITICAL(&config_guard);e.config=config;e.generation=generation;portEXIT_CRITICAL(&config_guard);
    return e;
}
void radio_config_load() {
    Preferences p;
    RadioConfig next;
    if(p.begin("camper_radio",true)) {
        struct Stored {uint32_t magic;RadioConfig data;} stored{};
        if(p.getBytesLength("config_v2")==sizeof(stored) &&
           p.getBytes("config_v2",&stored,sizeof(stored))==sizeof(stored) && stored.magic==0x52414432)
            next=stored.data;
        p.end();
    }
    next.host[sizeof(next.host)-1]=0;
    for(int i=0;i<3;++i){next.playlists[i][63]=0;next.dab_uris[i][255]=0;}
    if(!radio_config_valid(next))next=RadioConfig();
    portENTER_CRITICAL(&config_guard);config=next;++generation;portEXIT_CRITICAL(&config_guard);
}
bool radio_config_save(const RadioConfig &next) {
    if(!radio_config_valid(next))return false;
    auto old=radio_endpoint();
    if(!memcmp(&next,&old.config,sizeof(next)))return true;
    Preferences p;
    if(!p.begin("camper_radio",false))return false;
    // Single versioned blob is committed atomically; a power loss cannot mix fields.
    struct Stored {uint32_t magic;RadioConfig data;} stored{0x52414432,next};
    bool ok=p.putBytes("config_v2",&stored,sizeof(stored))==sizeof(stored);p.end();
    if(!ok)return false;
    portENTER_CRITICAL(&config_guard);config=next;++generation;portEXIT_CRITICAL(&config_guard);
    return true;
}
