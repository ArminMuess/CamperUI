#pragma once
#include <stdint.h>
#include <stddef.h>

constexpr bool gx_equal(const char *a,const char *b) {
    for(size_t i=0;;++i) { if(a[i]!=b[i])return false; if(!a[i])return true; }
}
constexpr bool gx_identifier(const char *s,size_t limit,bool allow_dot=false) {
    size_t i=0;
    for(;s[i];++i) {
        char c=s[i];
        if(i>=limit || !((c>='0'&&c<='9') || (c>='a'&&c<='z') || (c>='A'&&c<='Z') ||
            c=='-' || c=='_' || (allow_dot&&c=='.')))return false;
    }
    return true; // empty means automatic/unconfigured
}
struct GxTopic {
    char portal[33]={},service[24]={},path[96]={}; int instance=-1;
    bool valid=false, heartbeat=false;
};
constexpr GxTopic gx_parse_topic(const char *s) {
    GxTopic out;
    if(s[0]!='N'||s[1]!='/')return out;
    size_t pos=2;
    auto part=[&](char *dest,size_t size)->bool {
        size_t i=0;
        while(s[pos]&&s[pos]!='/') { if(i+1>=size)return false;dest[i++]=s[pos++]; }
        dest[i]=0; if(!i)return false; if(s[pos]=='/')++pos; return true;
    };
    if(!part(out.portal,sizeof(out.portal)) || !gx_identifier(out.portal,32))return out;
    if(!part(out.service,sizeof(out.service)))return out;
    if((gx_equal(out.service,"full_publish_completed") || gx_equal(out.service,"keepalive")) && !s[pos]) {
        out.valid=out.heartbeat=true;return out;
    }
    if(!gx_equal(out.service,"system") && !gx_equal(out.service,"vebus") && !gx_equal(out.service,"settings") && !gx_equal(out.service,"battery"))return out;
    int id=0; bool digits=false;
    while(s[pos]>='0'&&s[pos]<='9') { digits=true;id=id*10+s[pos++]-'0';if(id>65535)return out; }
    if(!digits||s[pos++]!='/')return out;
    out.instance=id;
    size_t i=0;for(;s[pos];++pos) { if(i+1>=sizeof(out.path))return out;out.path[i++]=s[pos]; }
    out.path[i]=0;out.valid=i>0;return out;
}
enum class GxCommandPhase:uint8_t { Idle,Queued,Sent,Confirmed,Timeout,Failed };
struct GxPending {
    GxCommandPhase phase=GxCommandPhase::Idle;
    int target=0; uint32_t session=0,requested=0,sent=0;
    constexpr bool active()const { return phase==GxCommandPhase::Queued||phase==GxCommandPhase::Sent; }
    constexpr void begin(int value,uint32_t epoch,uint32_t now) {
        target=value;session=epoch;requested=now;phase=GxCommandPhase::Queued;
    }
    constexpr bool mark_sent(uint32_t epoch,uint32_t now) {
        if(phase!=GxCommandPhase::Queued || epoch!=session)return false;
        sent=now;phase=GxCommandPhase::Sent;return true;
    }
    constexpr bool observe(int value,bool retained,uint32_t epoch,uint32_t now) {
        if(phase!=GxCommandPhase::Sent || retained || epoch!=session || value!=target || now-sent>=15000)return false;
        phase=GxCommandPhase::Confirmed;return true;
    }
    constexpr bool expire(uint32_t now) {
        if(!active() || now-(phase==GxCommandPhase::Sent?sent:requested)<15000)return false;
        phase=GxCommandPhase::Timeout;return true;
    }
};

struct GxSelection {
    int candidate=-1,preferred=-1;
    bool multiple=false,portal_conflict=false;
    constexpr bool ambiguous()const{return portal_conflict || (preferred<0 && multiple);}
    constexpr int observe(int id,bool system_preferred=false) {
        if(system_preferred)preferred=id;
        else if(preferred<0) {if(candidate<0)candidate=id;else if(candidate!=id)multiple=true;}
        return preferred>=0?preferred:multiple?-1:candidate;
    }
};

struct GxProbe {
    bool active=false;uint8_t needed=0,seen=0;uint32_t session=0,started=0;
    constexpr void begin(uint8_t mask,uint32_t epoch,uint32_t now) {
        active=true;needed=mask;seen=0;session=epoch;started=now;
    }
    constexpr void observe(uint8_t bit,bool retained,uint32_t epoch,uint32_t now) {
        if(active && !retained && session==epoch && now-started<15000)seen|=bit&needed;
    }
    constexpr bool complete()const{return active&&needed&&seen==needed;}
};

// Missing/retained evidence is distinct from an explicit device disconnect.
enum class GxControlStatus:uint8_t {MissingMode,MissingConnection,Disconnected,MissingPermission,ReadOnly,Ready,OffRemote};
constexpr GxControlStatus gx_control_status(bool mode_valid,bool mode_live,
    bool connected_valid,bool connected_live,int connected,
    bool permission_valid,bool permission_live,int permission,int mode=0) {
    if(!mode_valid||!mode_live)return GxControlStatus::MissingMode;
    if(!connected_valid||!connected_live)return GxControlStatus::MissingConnection;
    if(connected!=1 && !(connected==0&&mode==4))return GxControlStatus::Disconnected;
    if(!permission_valid||!permission_live)return GxControlStatus::MissingPermission;
    if(permission!=1)return GxControlStatus::ReadOnly;
    return connected==0?GxControlStatus::OffRemote:GxControlStatus::Ready;
}

// A remotely-off device can be woken only with explicit current GX permission.
// Receipt of all three values is required again by the pre-write probe.
constexpr bool gx_mode_control_allowed(bool transport_ready,int instance,
    bool mode_valid,bool mode_live,int mode,
    bool connected_valid,bool connected_live,int connected,
    bool permission_valid,bool permission_live,int permission) {
    if(!transport_ready || instance<0 || instance>65535 || mode<1 || mode>4)return false;
    auto status=gx_control_status(mode_valid,mode_live,connected_valid,connected_live,connected,
        permission_valid,permission_live,permission,mode);
    return status==GxControlStatus::Ready || status==GxControlStatus::OffRemote;
}
