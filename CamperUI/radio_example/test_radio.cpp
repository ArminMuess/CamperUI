#include "../volumio_protocol.h"
#include "../radio_tab_migration.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <set>
#include <cstdio>
#include <string>
using radio_protocol::Action;
int main() {
    using namespace radio_protocol;
    assert(path(Action::Play,0)=="/api/v1/commands/?cmd=play");
    assert(path(Action::Stop,0)=="/api/v1/commands/?cmd=stop");
    assert(path(Action::Volume,0)=="/api/v1/commands/?cmd=volume&volume=0");
    assert(path(Action::Volume,100)=="/api/v1/commands/?cmd=volume&volume=100");
    assert(path(Action::Volume,-1).empty() && path(Action::Volume,101).empty());
    assert(path(Action::Preset,-1,"x").empty() && path(Action::Preset,3,"x").empty());
    assert(path(Action::Preset,0,"").empty());
    assert(path(Action::Preset,1,"BOB! & Rock")=="/api/v1/commands/?cmd=playplaylist&name=BOB%21%20%26%20Rock");
    assert(encode("ä/#?+")=="%C3%A4%2F%23%3F%2B");
    assert(!expired(2499,0,2500) && expired(2500,0,2500));
    uint32_t wrap=std::numeric_limits<uint32_t>::max()-100;
    assert(!expired(100,wrap,2500) && expired(2400,wrap,2500));
    for(size_t count:{10U,11U,12U,13U}) for(bool legacy:{false,true}) {
        uint8_t order[15]={},expected[13]={};bool enabled[15]={},expected_en[13]={};
        for(size_t i=0;i<count;++i){order[i]=expected[i]=uint8_t(count-1-i);enabled[i]=expected_en[i]=(i%2==0);}
        rv_migrate_tab_order(expected,count,legacy);rv_migrate_tab_visibility(expected_en,count,legacy);
        assert(radio_migrate_tab_order(order,count,legacy)==15);
        assert(radio_migrate_tab_visibility(enabled,count,legacy)==15);
        assert(std::set<unsigned>(order,order+15).size()==15);
        for(size_t i=0;i<13;++i){assert(order[i]==expected[i]);assert(enabled[i]==expected_en[i]);}
        assert(order[13]==13 && enabled[13] && order[14]==14 && enabled[14]);
    }
    uint8_t order[15];bool enabled[15];
    // Saved by radio-dev.3 (14 tabs): user order kept, Power Unit appended and shown
    for(unsigned i=0;i<14;++i){order[i]=13-i;enabled[i]=false;}
    assert(radio_migrate_tab_order(order,14,false)==15 && order[0]==13 && order[13]==0 && order[14]==14);
    assert(radio_migrate_tab_visibility(enabled,14,false)==15 && !enabled[13] && enabled[14]);
    assert(std::set<unsigned>(order,order+15).size()==15);
    assert(radio_migrate_tab_order(order,15,false)==15 && order[0]==13);
    assert(radio_migrate_tab_order(order,9,false)==0);
    // Display text: keep umlauts/€, map punctuation and accents, never grow
    auto clean=[](const char *in){char b[160];snprintf(b,sizeof(b),"%s",in);display_text(b);return std::string(b);};
    assert(clean("Söhne Mannheims – Geh’ davon aus…")=="Söhne Mannheims - Geh' davon aus...");
    assert(clean("„Hörerwunsch“ für 5 €")=="\"Hörerwunsch\" für 5 €");
    assert(clean("Beyoncé · Café Señor Ça")=="Beyonce - Cafe Senor Ca");
    assert(clean("ÄÖÜäöüß")=="ÄÖÜäöüß");
    assert(clean("Emoji 🎵 ok")=="Emoji ? ok" && clean("日本")=="??");
    assert(clean("cut \xE2\x80")=="cut ?");   // truncated sequence at the end
    char tail[]="x\xC3"; display_text(tail); assert(std::string(tail)=="x?");
    // DAB names from plugin URIs
    char n[64];
    assert(dab_name("rtlsdr://dab/5C/RADIO%20BOB%21",n,sizeof(n)) && std::string(n)=="RADIO BOB!");
    assert(dab_name("rtlsdr://dab/11D/WDR%202%20%E2%80%93%20Rheinland",n,sizeof(n)) && std::string(n)=="WDR 2 - Rheinland");
    assert(dab_name("rtlsdr://dab/12A/Bayern%203%20%C3%96",n,sizeof(n)) && std::string(n)=="Bayern 3 Ö");
    assert(!dab_name("",n,sizeof(n)) && !*n);
    assert(!dab_name("rtlsdr://dab/5C/",n,sizeof(n)) && !dab_name("http://x/5C/a",n,sizeof(n)));
    char small[6]; assert(dab_name("rtlsdr://dab/5C/ABCDEFGH",small,sizeof(small)) && std::string(small)=="ABCDE");
    std::cout<<"PASS: display text cleanup, DAB names\n";
    // Radio-Pi power helpers
    {
        std::string frame=SHUTDOWN_FRAME; auto colon=frame.find(':');
        assert(std::stoul(frame.substr(0,colon))==frame.size()-colon-1);   // Engine.IO length prefix
        const char *hs="96:0{\"sid\":\"Xk2_bQ-9aZ\",\"upgrades\":[\"websocket\"],\"pingInterval\":25000,\"pingTimeout\":5000}2:40";
        assert(engineio_sid(hs)=="Xk2_bQ-9aZ");
        assert(engineio_sid("").empty() && engineio_sid("{\"sid\":\"\"}").empty() && engineio_sid("{\"sid\":\"a&b\"}").empty());
        bool on=false;
        assert(shelly_output("{\"id\":0,\"source\":\"HTTP_in\",\"output\":true,\"temperature\":{}}",on) && on);
        assert(shelly_output("{\"id\":0,\"output\": false}",on) && !on);
        assert(!shelly_output("{\"was_on\":true}",on));
        assert(std::string(shelly_set(true))=="/rpc/Switch.Set?id=0&on=true");
        bool armed=true;
        assert(!guard_trigger(50,25,armed) && !guard_trigger(25,25,armed));
        assert(guard_trigger(24,25,armed) && !armed);          // fires once
        assert(!guard_trigger(20,25,armed) && !guard_trigger(29,25,armed));  // no repeat, manual restart possible
        assert(!guard_trigger(30,25,armed) && armed);          // re-armed after +5
        assert(guard_trigger(10,25,armed));
        armed=true; assert(!guard_trigger(-1,25,armed) && !guard_trigger(0,25,armed) && !guard_trigger(5,0,armed));
    }
    std::cout<<"PASS: Volumio shutdown frame, Shelly status, battery guard\n";
    std::cout<<"PASS: REST commands, UTF-8 encoding, bounds, timeout wraparound, 8 migrations, saved settings\n";
}
