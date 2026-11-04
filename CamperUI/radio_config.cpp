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
        struct StoredV2 {uint32_t magic;RadioConfigV2 data;} old{};
        if(p.isKey("config_v3")) {
            // Current format; a damaged record falls back to defaults, never to older data.
            if(p.getBytesLength("config_v3")==sizeof(stored) &&
               p.getBytes("config_v3",&stored,sizeof(stored))==sizeof(stored) && stored.magic==0x52414433)
                next=stored.data;
        } else if(p.getBytesLength("config_v2")==sizeof(old) &&
                  p.getBytes("config_v2",&old,sizeof(old))==sizeof(old) && old.magic==0x52414432) {
            // radio-dev.2 settings: keep them, new fields start with their defaults.
            next.enabled=old.data.enabled;memcpy(next.host,old.data.host,sizeof(next.host));next.port=old.data.port;
            memcpy(next.playlists,old.data.playlists,sizeof(next.playlists));
            memcpy(next.dab_uris,old.data.dab_uris,sizeof(next.dab_uris));
        }
        p.end();
    }
    next.host[sizeof(next.host)-1]=0;next.shelly_host[sizeof(next.shelly_host)-1]=0;
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
    struct Stored {uint32_t magic;RadioConfig data;} stored{0x52414433,next};
    bool ok=p.putBytes("config_v3",&stored,sizeof(stored))==sizeof(stored);p.end();
    if(!ok)return false;
    portENTER_CRITICAL(&config_guard);config=next;++generation;portEXIT_CRITICAL(&config_guard);
    return true;
}
