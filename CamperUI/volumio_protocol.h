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
// The display fonts cover ASCII plus Ä Ö Ü ä ö ü ß €. Station metadata often contains
// typographic punctuation and accented letters; map them in place to drawable text so
// no empty boxes appear. Output is never longer than input.
inline void display_text(char *s) {
    static const char latin1[] = // U+00C0..U+00FF; 0 = keep (umlaut/ß), '?' = no good match
        "AAAAAAACEEEEIIII" "DNOOOOOxOUUUUYTs"   // C0..DF (C4 Ä, D6 Ö, DC Ü, DF ß kept below)
        "aaaaaaaceeeeiiii" "dnooooo/ouuuuyty";  // E0..FF (E4 ä, F6 ö, FC ü kept below)
    unsigned char *r = reinterpret_cast<unsigned char *>(s), *w = r;
    while (*r) {
        unsigned char c = *r;
        if (c < 0x80) { *w++ = *r++; continue; }
        // Kept glyphs: Ä Ö Ü ß ä ö ü (C3 xx) and € (E2 82 AC)
        if (c == 0xC3 && r[1] && (r[1] == 0x84 || r[1] == 0x96 || r[1] == 0x9C || r[1] == 0x9F ||
                                  r[1] == 0xA4 || r[1] == 0xB6 || r[1] == 0xBC)) {
            *w++ = *r++; *w++ = *r++; continue;
        }
        if (c == 0xE2 && r[1] == 0x82 && r[2] == 0xAC) { *w++ = *r++; *w++ = *r++; *w++ = *r++; continue; }
        if (c == 0xC3 && r[1] >= 0x80 && r[1] <= 0xBF) { *w++ = latin1[r[1] - 0x80]; r += 2; continue; }
        if (c == 0xC2 && r[1] >= 0x80 && r[1] <= 0xBF) {   // NBSP, «, », ´ ...
            unsigned char d = r[1];
            *w++ = d == 0xA0 ? ' ' : (d == 0xAB || d == 0xBB) ? '"' : d == 0xB4 ? '\'' : d == 0xB7 ? '-' : '?';
            r += 2; continue;
        }
        if (c == 0xE2 && r[1] == 0x80 && r[2]) {           // General punctuation U+2000..U+203F
            unsigned char d = r[2];
            if (d == 0xA6) { *w++ = '.'; *w++ = '.'; *w++ = '.'; }
            else *w++ = (d >= 0x90 && d <= 0x95) ? '-' : (d >= 0x98 && d <= 0x9B) ? '\'' :
                        (d >= 0x9C && d <= 0x9F) ? '"' : (d == 0xA2 || d == 0xA7) ? '-' : ' ';
            r += 3; continue;
        }
        // Anything else (other scripts, emoji): one '?' per code point.
        size_t n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        for (size_t i = 0; i < n && *r; ++i) ++r;
        *w++ = '?';
    }
    *w = 0;
}
// Station name from a DAB URI of the FM/DAB plugin: rtlsdr://dab/<channel>/<encoded name>
inline bool dab_name(const char *uri, char *out, size_t len) {
    if (!len) return false;
    out[0] = 0;
    const char prefix[] = "rtlsdr://dab/";
    if (strncmp(uri, prefix, sizeof(prefix) - 1)) return false;
    const char *slash = strchr(uri + sizeof(prefix) - 1, '/');
    if (!slash || !slash[1]) return false;
    auto hex = [](char h) { return h >= '0' && h <= '9' ? h - '0' : h >= 'A' && h <= 'F' ? h - 'A' + 10 :
                                   h >= 'a' && h <= 'f' ? h - 'a' + 10 : -1; };
    size_t o = 0;
    for (const char *p = slash + 1; *p && o + 1 < len; ++p) {
        if (*p == '%' && hex(p[1]) >= 0 && hex(p[2]) >= 0) { out[o++] = char(hex(p[1]) * 16 + hex(p[2])); p += 2; }
        else out[o++] = *p;
    }
    out[o] = 0;
    display_text(out);
    return o > 0;
}
// ---- Radio-Pi power (Shelly Gen3 relay + Volumio shutdown) -------------------------
// Volumio has no REST shutdown; its socket.io 2.x interface (Engine.IO v3) accepts the
// "shutdown" event. Over HTTP long-polling that is: handshake GET, then one POST.
constexpr const char *ENGINEIO_PATH = "/socket.io/?EIO=3&transport=polling&b64=1";
constexpr const char *SHUTDOWN_FRAME = "14:42[\"shutdown\"]";   // <length>:<packet>
inline std::string engineio_sid(const char *body) {
    const char *p = strstr(body, "\"sid\":\"");
    if (!p) return {};
    p += 7;
    const char *end = strchr(p, '"');
    if (!end || end == p || end - p > 64) return {};
    std::string sid(p, end);
    for (char c : sid)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return {};
    return sid;
}
// Shelly Gen2/Gen3 local RPC
constexpr const char *SHELLY_STATUS = "/rpc/Switch.GetStatus?id=0";
inline const char *shelly_set(bool on) { return on ? "/rpc/Switch.Set?id=0&on=true" : "/rpc/Switch.Set?id=0&on=false"; }
// Reads "output": true/false from a Switch.GetStatus answer.
inline bool shelly_output(const char *json, bool &on) {
    const char *p = strstr(json, "\"output\"");
    if (!p) return false;
    p += 8;
    while (*p == ' ' || *p == ':') ++p;
    if (!strncmp(p, "true", 4)) { on = true; return true; }
    if (!strncmp(p, "false", 5)) { on = false; return true; }
    return false;
}
// Battery guard fires once when the charge drops below the threshold and re-arms
// only after it has recovered by 5 points, so a manual restart stays possible.
inline bool guard_trigger(int soc, int threshold, bool &armed) {
    if (threshold <= 0 || soc <= 0 || soc > 100) return false;
    if (soc >= threshold + 5) { armed = true; return false; }
    if (soc < threshold && armed) { armed = false; return true; }
    return false;
}
// Shutdown timing (ms)
constexpr unsigned POWER_CHECK_MS = 2000, POWER_DOWN_SETTLE_MS = 15000,
                   POWER_STOP_TIMEOUT_MS = 120000, POWER_START_TIMEOUT_MS = 180000;
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
