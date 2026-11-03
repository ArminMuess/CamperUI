#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
struct RadioConfig {
    bool enabled = true;
    char host[64] = "";
    uint16_t port = 80;
    char playlists[3][64] = {"Camper hr3", "Camper Radio BOB", "Camper Rockantenne"};
    char dab_uris[3][256] = {};
};
// Plain host only, no scheme/path/credentials. IPv4 and DNS names are supported.
inline bool radio_valid_host(const char *s) {
    size_t n = strlen(s);
    if (!n) return true;
    if (n > 63 || s[0] == '.' || s[n-1] == '.') return false;
    size_t label = 0;
    for (size_t i=0; i<n; ++i) {
        char c=s[i];
        if (c=='.') { if (!label || s[i-1]=='-') return false; label=0; }
        else {
            if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')) return false;
            if (!label && c=='-') return false;
            ++label;
        }
    }
    return label && s[n-1]!='-';
}
inline bool radio_valid_dab_uri(const char *s) {
    if (!*s) return true;
    const char prefix[]="rtlsdr://dab/";
    if (strncmp(s,prefix,sizeof(prefix)-1)) return false;
    const char *channel=s+sizeof(prefix)-1, *slash=strchr(channel,'/');
    if (!slash || slash==channel || slash-channel>3 || !slash[1]) return false;
    for (const char *q=channel;q<slash;++q) if (!((*q>='0'&&*q<='9')||(*q>='A'&&*q<='Z'))) return false;
    for (const unsigned char *q=(const unsigned char*)slash+1;*q;++q)
        if (*q<=32 || *q==127 || *q=='"' || *q=='\\' || *q=='#' || *q=='?') return false;
    return strlen(s)<256;
}
inline bool radio_config_valid(const RadioConfig &c) {
    if (!radio_valid_host(c.host) || !c.port) return false;
    for(int i=0;i<3;++i) if (!*c.playlists[i] || !radio_valid_dab_uri(c.dab_uris[i])) return false;
    return true;
}
struct RadioEndpoint { RadioConfig config; uint32_t generation=0; };
RadioEndpoint radio_endpoint();
void radio_config_load();
bool radio_config_save(const RadioConfig &config);
