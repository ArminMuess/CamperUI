#include "victron_protocol.h"
#include "settings_migration.h"
constexpr bool parse_real_topics() {
    auto mode=gx_parse_topic("N/aabbccddeeff/vebus/257/Mode");
    auto relay=gx_parse_topic("N/aabbccddeeff/system/0/Relay/1/State");
    auto function=gx_parse_topic("N/aabbccddeeff/settings/0/Settings/Relay/Function");
    auto full=gx_parse_topic("N/aabbccddeeff/full_publish_completed");
    return mode.valid && mode.instance==257 && gx_equal(mode.path,"Mode") &&
        relay.valid && relay.instance==0 && gx_equal(relay.path,"Relay/1/State") &&
        function.valid && gx_equal(function.service,"settings") && full.valid && full.heartbeat;
}
constexpr bool reject_wrong_topics() {
    return !gx_parse_topic("W/aabb/vebus/257/Mode").valid &&
        !gx_parse_topic("N/aabb/vebus/999999/Mode").valid &&
        !gx_parse_topic("N/aabb/vebus/-1/Mode").valid &&
        !gx_parse_topic("N/+/vebus/257/Mode").valid &&
        !gx_parse_topic("N/aabb/solarcharger/257/Mode").valid &&
        !gx_parse_topic("N/aabb/vebus/257/").valid && !gx_parse_topic("").valid;
}
constexpr bool only_new_feedback_confirms() {
    GxPending p;p.begin(4,7,100);
    if(p.observe(4,false,7,101))return false; // queued, not sent
    if(!p.mark_sent(7,110))return false;
    if(p.observe(4,true,7,120)||p.observe(3,false,7,120)||p.observe(4,false,6,120))return false;
    return p.observe(4,false,7,130)&&p.phase==GxCommandPhase::Confirmed;
}
constexpr bool no_late_confirmation() {
    GxPending p;p.begin(1,8,0);p.mark_sent(8,20);
    if(p.observe(1,false,8,15020))return false;
    return p.expire(15020) && !p.active() && !p.observe(1,false,8,15021);
}
constexpr bool time_rollover() {
    GxPending p;p.begin(1,1,0xfffffff0u);p.mark_sent(1,0xfffffff0u);
    return !p.expire(14983) && p.expire(14984) && p.phase==GxCommandPhase::Timeout;
}
constexpr bool topic_injection_blocked() {
    return gx_identifier("aabb01234567",32) && gx_identifier("",32) &&
        !gx_identifier("aabb/vebus/257/Mode",32) && !gx_identifier("+",32) &&
        !gx_identifier("#",32) && !gx_identifier(" bad",32) &&
        gx_identifier("venus-aabb.local",63,true) && !gx_identifier("mqtt://gx",63,true);
}
static_assert(parse_real_topics(),"official Victron topic shapes must resolve correctly");
static_assert(reject_wrong_topics(),"writes, other services and invalid instances are not feedback");
static_assert(only_new_feedback_confirms(),"retained, queued, wrong-session and wrong-target feedback must not confirm");
static_assert(no_late_confirmation(),"timeouts cannot turn into apparent success");
static_assert(time_rollover(),"timeout must survive millis overflow");
static_assert(topic_injection_blocked(),"config cannot inject MQTT wildcards or topic paths");

constexpr bool preserved_tab_choices() {
    uint8_t order[12]={};bool enabled[12]={};
    for(int i=0;i<10;++i){order[i]=9-i;enabled[i]=i%2==0;}
    if(rv_migrate_tab_order(order,10,false)!=12||rv_migrate_tab_visibility(enabled,10,false)!=12)return false;
    for(int i=0;i<10;++i)if(order[i]!=9-i||enabled[i]!=(i%2==0))return false;
    return order[10]==10 && order[11]==11 && !enabled[10] && !enabled[11];
}
constexpr bool legacy_victron_identity() {
    uint8_t order[12]={10,0,1,2,3,4,5,6,7,8,9};bool enabled[12]={};enabled[10]=true;
    rv_migrate_tab_order(order,11,true);rv_migrate_tab_visibility(enabled,11,true);
    return order[0]==11 && order[11]==10 && enabled[11] && !enabled[10];
}
constexpr bool upstream_truma_identity() {
    uint8_t order[12]={10,9,8,7,6,5,4,3,2,1,0};bool enabled[12]={};enabled[10]=true;
    rv_migrate_tab_order(order,11,false);rv_migrate_tab_visibility(enabled,11,false);
    if(order[0]!=10 || order[11]!=11 || !enabled[10] || enabled[11])return false;
    return rv_migrate_tab_order(order,12,true)==12 && order[0]==10 && order[11]==11;
}
static_assert(legacy_victron_identity(),"legacy RV3D VE visibility and position must survive the Truma ID collision");
static_assert(upstream_truma_identity(),"stock V1.6.1 Truma visibility and position must remain intact");
static_assert(rv_migrate_heater_type(2,true)==3 && rv_migrate_heater_type(2,false)==2 &&
    rv_migrate_heater_type(1,true)==1,"legacy disabled heater must not turn into Truma Electric");
constexpr bool device_selection() {
    GxSelection ids;
    if(ids.observe(257)!=257 || ids.ambiguous())return false;
    if(ids.observe(258)!=-1 || !ids.ambiguous())return false;
    if(ids.observe(258,true)!=258 || ids.ambiguous())return false;
    ids.portal_conflict=true;
    return ids.observe(257,true)==257 && ids.ambiguous();
}
static_assert(preserved_tab_choices(),"old tab order and hidden pages must survive migration to twelve tabs");
static_assert(device_selection(),"system selection resolves multi-device ambiguity but never portal ambiguity");

constexpr bool permission_probe() {
    GxProbe p;p.begin(3,7,100);
    p.observe(1,true,7,110);p.observe(2,false,6,110);
    if(p.complete()||p.seen)return false;
    p.observe(1,false,7,111);if(p.complete())return false;
    p.observe(2,false,7,112);return p.complete();
}
constexpr bool old_permission_not_accepted() {
    GxProbe p;p.begin(7,8,200);p.observe(7,false,8,199);
    if(p.complete())return false;
    p.observe(7,false,8,15200);if(p.complete())return false;
    p.observe(7,false,8,201);return p.complete();
}
static_assert(permission_probe(),"each write needs fresh state plus current permission/role, not retained data");
static_assert(old_permission_not_accepted(),"old or timed-out permission responses are insufficient");

constexpr bool starter_topics_and_selection() {
    auto starter=gx_parse_topic("N/aabbccddeeff/battery/512/Dc/1/Voltage");
    auto connected=gx_parse_topic("N/aabbccddeeff/battery/512/Connected");
    GxSelection selection;
    if(selection.observe(512)!=512 || selection.ambiguous())return false;
    selection.observe(513);
    return starter.valid && connected.valid && starter.instance==512 &&
        gx_equal(starter.service,"battery") && gx_equal(starter.path,"Dc/1/Voltage") && selection.ambiguous();
}
static_assert(starter_topics_and_selection(),"starter reads use battery auxiliary voltage and reject ambiguous automatic selection");

constexpr bool control_diagnostics() {
    return gx_control_status(false,false,false,false,0,false,false,0)==GxControlStatus::MissingMode &&
        gx_control_status(true,true,false,false,0,true,true,1)==GxControlStatus::MissingConnection &&
        gx_control_status(true,true,true,false,1,true,true,1)==GxControlStatus::MissingConnection &&
        gx_control_status(true,true,true,true,0,true,true,1)==GxControlStatus::Disconnected &&
        gx_control_status(true,true,true,true,1,false,false,0)==GxControlStatus::MissingPermission &&
        gx_control_status(true,true,true,true,1,true,false,1)==GxControlStatus::MissingPermission &&
        gx_control_status(true,true,true,true,1,true,true,0)==GxControlStatus::ReadOnly &&
        gx_control_status(true,true,true,true,1,true,true,1)==GxControlStatus::Ready;
}
static_assert(control_diagnostics(),"missing or retained readback must not be reported as explicit disconnected hardware");

constexpr bool remote_off_can_wake() {
    if(!gx_mode_control_allowed(true,276,true,true,4,true,true,0,true,true,1))return false;
    if(!gx_mode_control_allowed(true,276,true,true,3,true,true,1,true,true,1))return false;
    if(!gx_mode_control_allowed(true,276,true,true,2,true,true,1,true,true,1))return false;
    return gx_control_status(true,true,true,true,0,true,true,1,4)==GxControlStatus::OffRemote;
}
constexpr bool wake_requires_current_evidence() {
    return !gx_mode_control_allowed(false,276,true,true,4,true,true,0,true,true,1) &&
        !gx_mode_control_allowed(true,-1,true,true,4,true,true,0,true,true,1) &&
        !gx_mode_control_allowed(true,276,false,false,4,true,true,0,true,true,1) &&
        !gx_mode_control_allowed(true,276,true,false,4,true,true,0,true,true,1) &&
        !gx_mode_control_allowed(true,276,true,true,4,false,false,0,true,true,1) &&
        !gx_mode_control_allowed(true,276,true,true,4,true,false,0,true,true,1) &&
        !gx_mode_control_allowed(true,276,true,true,4,true,true,0,false,false,1) &&
        !gx_mode_control_allowed(true,276,true,true,4,true,true,0,true,false,1) &&
        !gx_mode_control_allowed(true,276,true,true,4,true,true,0,true,true,0);
}
constexpr bool other_disconnected_modes_stay_blocked() {
    for(int mode=0;mode<=5;++mode) {
        if(mode!=4 && gx_mode_control_allowed(true,276,true,true,mode,true,true,0,true,true,1))return false;
    }
    return !gx_mode_control_allowed(true,276,true,true,4,true,true,2,true,true,1);
}
constexpr bool wake_probe_and_feedback() {
    GxProbe probe;GxPending pending;
    probe.begin(7,7,100);pending.begin(3,7,100);
    probe.observe(1,false,7,101);probe.observe(2,false,7,102);
    if(probe.complete() || pending.observe(3,false,7,103))return false;
    probe.observe(4,true,7,103);if(probe.complete())return false;
    probe.observe(4,false,7,104);if(!probe.complete())return false;
    if(!gx_mode_control_allowed(true,276,true,true,4,true,true,0,true,true,1))return false;
    if(!pending.mark_sent(7,105) || pending.observe(4,false,7,106) || pending.observe(3,true,7,106))return false;
    return pending.observe(3,false,7,107) && pending.phase==GxCommandPhase::Confirmed;
}
static_assert(remote_off_can_wake(),"remote-off mode with explicit GX permission can be woken");
static_assert(wake_requires_current_evidence(),"no wake on stale/offline/missing/retained/unpermitted values");
static_assert(other_disconnected_modes_stay_blocked(),"disconnected modes other than OFF remain blocked");
static_assert(wake_probe_and_feedback(),"waking still requires complete fresh preflight and new target feedback");
