#include "../radio_config.h"
#include "../volumio_protocol.h"
#include <cassert>
#include <iostream>
int main(){
    assert(radio_valid_host("") && radio_valid_host("192.168.8.123") && radio_valid_host("volumio.local"));
    for(auto host:{"http://volumio","a/b","a:80","a@b","a b","a..b","-a","a-","a."})assert(!radio_valid_host(host));
    assert(radio_valid_dab_uri("") && radio_valid_dab_uri("rtlsdr://dab/5C/RADIO%20BOB%21"));
    for(auto uri:{"https://x","rtlsdr://fm/99","rtlsdr://dab/ensemble/foo","rtlsdr://dab/5C/","rtlsdr://dab/5C/Radio Bob","rtlsdr://dab/5C/a#b"})assert(!radio_valid_dab_uri(uri));
    RadioConfig c;assert(radio_config_valid(c));c.port=0;assert(!radio_config_valid(c));c.port=80;
    c.playlists[1][0]=0;assert(!radio_config_valid(c));
    using namespace radio_protocol;
    assert(source("spop","")==Source::Spotify);
    assert(source("spotify","")==Source::Spotify);
    assert(source("rtlsdr_radio","rtlsdr://dab/5C/Rock")==Source::Dab);
    assert(source("mpd","http://stream")==Source::Other);
    assert(!current_command(100,0,2500,1,2));
    assert(!current_command(2500,0,2500,2,2));
    assert(current_command(200,0,2500,2,2));
    assert(path(Action::Pause,0)=="/api/v1/commands/?cmd=pause");
    assert(path(Action::Next,0)=="/api/v1/commands/?cmd=next");
    assert(path(Action::Previous,0)=="/api/v1/commands/?cmd=prev");
    assert(path(Action::DabPreset,0)=="/api/v1/replaceAndPlay");
    assert(path(Action::DabPreset,3).empty());
    std::cout<<"PASS: host/DAB configuration, sources, endpoint generation and expiry, Spotify/DAB command paths\n";
}
