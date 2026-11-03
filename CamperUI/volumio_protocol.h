#pragma once
#include <stdint.h>
#include <string>
#include <string.h>
namespace radio_protocol {
enum class Action : uint8_t { Play, Stop, Volume, Preset, Pause, Next, Previous, DabPreset };
inline bool valid(Action a, int value) {
    return a == Action::Play || a == Action::Stop || a == Action::Pause || a == Action::Next || a == Action::Previous ||
        (a == Action::Volume && value >= 0 && value <= 100) ||
        ((a == Action::Preset || a == Action::DabPreset) && value >= 0 && value < 3);
}
inline bool expired(uint32_t now, uint32_t created, uint32_t ttl) {
    return uint32_t(now - created) >= ttl;
}
inline bool current_command(uint32_t now,uint32_t created,uint32_t ttl,uint32_t queued_generation,uint32_t active_generation) {
    return queued_generation==active_generation && !expired(now,created,ttl);
}
enum class Source { Other, Spotify, Dab };
inline Source source(const char *service,const char *uri) {
    if(!strcmp(service,"spop")||!strcmp(service,"spotify"))return Source::Spotify;
    if(!strncmp(uri,"rtlsdr://dab/",13))return Source::Dab;
    return Source::Other;
}
inline std::string encode(const char *s) {
    const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s); *p; ++p) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out += char(c);
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
inline std::string path(Action action, int value, const char *playlist = "") {
    if (!valid(action, value)) return {};
    std::string base = "/api/v1/commands/?cmd=";
    switch (action) {
        case Action::Play: return base + "play";
        case Action::Stop: return base + "stop";
        case Action::Pause: return base + "pause";
        case Action::Next: return base + "next";
        case Action::Previous: return base + "prev";
        case Action::DabPreset: return "/api/v1/replaceAndPlay";
        case Action::Volume: return base + "volume&volume=" + std::to_string(value);
        case Action::Preset: return *playlist ? base + "playplaylist&name=" + encode(playlist) : std::string();
    }
    return {};
}
}
