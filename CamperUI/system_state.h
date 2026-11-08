#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <Preferences.h>
#include "heater_diagnostics.h"

enum TabId : uint8_t {
    TAB_HOME = 0,
    TAB_DIMMERS = 1,
    TAB_POWER = 2,
    TAB_WATER = 3,
    TAB_CLIMATE = 4,
    TAB_TEMPS = 5,
    TAB_MAXXFAN = 6,
    TAB_SWITCHES = 7,
    TAB_LEVEL = 8,
    TAB_SETTINGS = 9,
    TAB_TRUMA = 10,
    TAB_VICTRON = 11, // appended after upstream Truma; legacy RV3D IDs migrate on load
    TAB_FRIDGE = 12,  // Dometic RC10 CI-Bus Fridge
    TAB_RADIO = 13,   // local Volumio extension; existing IDs stay stable
    TAB_POWERUNIT = 14, // Pekaway Power Unit via MQTT
    TAB_COUNT = 15
};

enum HeaterType : uint8_t {
    HEATER_TYPE_AUTOTERM = 0,
    HEATER_TYPE_TRUMA = 1,
    HEATER_TYPE_TRUMA_ELECTRIC = 2,
    HEATER_TYPE_NONE = 3,
    HEATER_TYPE_WEBASTO = 4
};

inline bool is_truma_heater(uint8_t type) {
    return (type == HEATER_TYPE_TRUMA || type == HEATER_TYPE_TRUMA_ELECTRIC);
}

inline bool truma_has_electric(uint8_t type) {
    return (type == HEATER_TYPE_TRUMA_ELECTRIC);
}

inline bool is_webasto_heater(uint8_t type) {
    return (type == HEATER_TYPE_WEBASTO);
}

// Pre-configuration block that can be patched directly in the binary before flashing
struct __attribute__((packed)) PreFlashingConfig {
    char magic[16];             // "CAMPERUI_CFG_V1" (null-terminated)
    uint8_t is_configured;      // 0 = unconfigured, 1 = pre-configured in browser
    char wifi_ssid[64];
    char wifi_pass[64];
    char vanpi_ip[64];
    int16_t display_brightness; // 10..100 (-1 = default)
    int8_t display_rotation;    // 0..3 (-1 = default)
    int8_t dark_mode;           // 0 = light, 1 = dark, -1 = default
    uint8_t reserved[48];       // reserved for future expansion
};

extern const volatile PreFlashingConfig PRE_FLASH_CONFIG;

// Custom Theme Configuration (Persisted)
struct ThemeConfig {
    uint8_t version;           // 3
    char name[32];             // e.g. "Camper Slate"
    uint32_t color_primary;    // e.g. 0x0ea5e9 (Accent / Actions / Buttons)
    uint32_t color_dimmer;     // e.g. 0xf59e0b (Dimmer Slider)
    uint32_t color_relay;      // e.g. 0x10b981 (Schalter / Relais aktiv)
    uint32_t color_tank;       // e.g. 0x06b6d4 (Füllstände / Wassertanks)
    uint32_t color_bg;         // e.g. 0x101318 (Display BG)
    uint32_t color_card;       // e.g. 0x181c24 (Card & Header BG)
    uint32_t color_border;     // e.g. 0x272d3b (Border)
    uint32_t color_text;       // e.g. 0xf1f5f9 (Main text)
    uint32_t color_muted;      // e.g. 0x94a3b8 (Muted text)
    uint32_t color_track;      // e.g. 0x12151c (Track / Slider BG)
    uint32_t color_btn_text;        // e.g. 0xf1f5f9 (Allgemeine Button-Schriftfarbe)
    uint32_t color_btn_text_active; // e.g. 0xffffff (Schriftfarbe aktiver Buttons)
    uint8_t radius_card;       // e.g. 14 (0..30)
    uint8_t radius_button;     // e.g. 10 (0..20)
    uint8_t border_width;      // e.g. 1 (0..3)
};

inline void theme_config_init_default(ThemeConfig &theme) {
    theme.version = 3;
    memset(theme.name, 0, sizeof(theme.name));
    strncpy(theme.name, "Camper Slate", sizeof(theme.name) - 1);
    theme.color_primary = 0x0ea5e9;
    theme.color_dimmer  = 0xf59e0b;
    theme.color_relay   = 0x10b981;
    theme.color_tank    = 0x06b6d4;
    theme.color_bg      = 0x101318;
    theme.color_card    = 0x181c24;
    theme.color_border  = 0x272d3b;
    theme.color_text    = 0xf1f5f9;
    theme.color_muted   = 0x94a3b8;
    theme.color_track   = 0x12151c;
    theme.color_btn_text        = 0xf1f5f9;
    theme.color_btn_text_active = 0xffffff;
    theme.radius_card   = 14;
    theme.radius_button = 10;
    theme.border_width  = 1;
}

// Global State Structure to hold all sensor data and system settings
static constexpr int TEMP_SOURCE_COUNT = 13; // temp1..4, ruuvitag0..8 (stable persisted indices)
struct SystemState {
    // Custom Theme
    ThemeConfig theme;

    // --- Settings (Persisted) ---
    String wifi_ssid;
    String wifi_pass;
    String vanpi_ip;
    
    // Menu / Navigation Customization (Persisted)
    uint8_t tab_order[TAB_COUNT];
    bool tab_enabled[TAB_COUNT];
    
    int display_brightness; // 10-100
    int display_timeout;
    int display_rotation; // 0, 1, 2, 3
    bool dark_mode;
    bool battery_icon_mode; // true = icon, false = percent
    bool buzzer_enabled;    // Touch-tone / haptic buzzer feedback
    bool screensaver_enabled; // true = display screensaver with 1% brightness on timeout
    int screensaver_brightness; // 1-100 (backlight brightness in %, default 5%)
    
    // Limits & Capacities
    float bat_capacity_ah;
    float solar_max_w;
    float shore_max_w;
    float alt_max_w;

    // Smart Header Alarm Thresholds (Persisted)
    float warn_frost_temp;
    int warn_bat_soc;
    int warn_bat_yellow_soc;
    int warn_fresh_min;
    int warn_waste_max;

    // Sensor Selection (Persisted)
    int outdoor_temp_sensor; // 0..12 (index of temp sensor for outdoor temp, default 1)
    int indoor_temp_sensor;  // 0..12 (index of temp sensor for indoor temp, default 0)
    int fridge_temp_sensor;  // -1 = None, 0..12 (index of temp sensor for fridge, default -1)
    int temps_custom_sensor[2]; // -1 hidden, otherwise /temp sensor 0..12
    int temps_history_window; // 1, 6, or 12 hours
    bool home_show_temp[3]; // shared Indoor, Outdoor, Custom 1 assignment
    int home_favorite[2]; // 0 hidden, 1..8 relays, 9..16 dimmers, 17..18 GX relays
    bool home_show_battery;
    bool home_show_starter;
    bool home_show_solar;
    bool home_show_water;
    bool home_show_victron;
    bool victron_enabled;
    bool victron_keepalive;
    bool victron_starter; // Home starter voltage: false = VanPi, true = GX battery auxiliary input
    int victron_battery_instance; // -1 automatic, independent of VE.Bus instance
    String victron_host, victron_portal, victron_user, victron_password;
    uint16_t victron_port;
    int victron_instance; // -1 automatic, otherwise VE.Bus instance
    String victron_relay_names[2];
    int home_water_source; // /level index 0..3

    // Power Dashboard Producers & Calculation (Persisted)
    bool power_show_solar;
    bool power_show_shore;
    bool power_show_alt;
    bool power_calc_load; // true: load = producers - net_battery, false: load = direct battery discharge only

    // Tank Capacities & Display Settings
    int tank_count;        // 1..4 tanks shown
    bool tank_show_liters; // true: show liters below %
    int tank_max[4];       // Capacities in Liters or kg
    uint8_t tank_unit[4];  // 0 = "L", 1 = "kg"
    bool tank_enabled[4];
    String tank_names[4];
    uint8_t heater_type;   // 0 = Autoterm, 1 = Truma, 2 = Truma + Elektro, 3 = None
    bool heater_temp_buttons; // true: show round +/- buttons next to temperature arc
    
    // Custom Names
    String switch_names[16];
    String dimmer_names[10];

    // WiFi Relays (Persisted)
    bool show_wrelay;
    bool wrelay_visible[8];
    String wrelay_names[8];

    // Hardware Extensions (Persisted)
    bool show_relay_ext;   // out-x-in Pro adds Relais 9-16
    bool dimmy_pro;        // Dimmy Pro uses channels 8-15

    // Firmware Update / Web OTA (Volatile)
    bool web_ota_active;
    int web_ota_progress;
    String web_ota_msg;

    // --- Live Sensor Data (Volatile) ---
    // Status
    bool wifi_connected;
    int wifi_rssi;
    bool vanpi_connected;
    String vanpi_current_date; // "14/10/2026, 18:30:35"
    String time_to_go;         // "7d, 14h" from /info ttgo
    

    // Power
    float bat_voltage;
    float bat_current;
    int bat_soc; // 0-100%
    float solar_power; // W
    float solar_current;
    float solar_voltage;
    float starter_voltage; // optional /batt extension: starter_voltage
    float shore_power;     // W
    float shore_voltage;   // V
    float shore_current;   // A
    bool shore_connected;  // true if 230V Landstrom connected
    bool shore_available;  // true if /shore endpoint is reachable
    float alt_power;       // W (B2B booster / Lima)
    float alt_current;     // A
    bool alt_active;       // true if alternator / booster active
    bool alt_available;    // true if /booster endpoint is reachable
    uint32_t bat_soc_updated_ms, bat_voltage_updated_ms, bat_current_updated_ms, solar_power_updated_ms;
    uint8_t battery_fields; // VoltB=1, Ampere=2, battsoc=4, optional starter_voltage=8
    uint8_t solar_fields; // watts=1, amps=2, volts=4
    uint16_t temp_fields;
    uint8_t tank_fields;
    uint16_t relay_fields;
    uint16_t dimmer_fields;
    bool data_is_demo; // source of the received fields, not the requested mode

    // Water
    int tank_level[4]; // 0-100%

    // Climate
    float indoor_temp;
    float outdoor_temp;
    float fridge_temp;
    float indoor_humidity;
    float target_temp;
    float temp_sensors[TEMP_SOURCE_COUNT];
    float temp_humidity[TEMP_SOURCE_COUNT];
    bool temp_is_humidity[TEMP_SOURCE_COUNT];
    String temp_sensor_names[TEMP_SOURCE_COUNT];
    bool heating_on;
    bool heater_vent_mode;
    bool heater_power_mode;
    int heater_power_level;
    String heater_status;   // raw "heatstatus" from VanPi (e.g. "standby", "heating")
    HeaterDiagnostics heater_diagnostics;
    String heater_error;    // raw "heaterror" from VanPi ("no" = no error)
    bool fan_on;

    // Webasto Live Telemetry
    float webasto_temp;
    float webasto_voltage;

    // Truma Climate & Boiler
    bool truma_online;
    String truma_model;
    bool truma_heating_on;
    float truma_target_temp;
    String truma_air_mode;
    String truma_water_mode;
    String truma_energy_mode;
    int truma_power_limit;
    bool truma_230v;
    String truma_error;
    uint32_t truma_hold_until;

    // Dometic RC10 CI-Bus Fridge
    bool fridge_power;
    uint8_t fridge_mode;         // 0 = performance, 1 = silent, 2 = turbo
    uint8_t fridge_step;         // 1..5
    char fridge_power_source[8]; // "DC12", "DC24", "AC", "unk"
    bool fridge_compressor;
    bool fridge_fan;
    char fridge_error[32];
    uint32_t fridge_last_update;
    uint32_t fridge_hold_until;

    // Inclinometer / Leveling
    float pitch_angle;
    float roll_angle;

    bool debug_mode;

    // Switches & Dimmers
    bool switch_state[16];
    bool switch_locked[16];
    int dimmer_val[10]; // 0-100%

    // Local-interaction hold-off (millis timestamps, not persisted):
    // while millis() < *_hold_until, polled values are ignored so the
    // remote state does not fight with a value the user is just changing.
    uint32_t dimmer_hold_until[10];
    uint32_t relay_hold_until[16];
    uint32_t heater_hold_until;

    // Visibility and roles
    bool switch_visible[16];
    bool dimmer_visible[10];
    int pump_relay; // -1 for none, 0-15 for relays, 16-23 for wrelays
    int drain_relay; // -1 for none, 0-15 for relays, 16-23 for wrelays

    // WiFi Relays (Volatile State)
    bool wrelay_state[8];
    bool wrelay_locked[8];
    uint32_t wrelay_hold_until[8];
    uint8_t wrelay_fields;
};

extern SystemState state;
extern Preferences prefs;
extern SemaphoreHandle_t state_mutex;

inline void state_lock() {
    if (state_mutex) xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
}

inline void state_unlock() {
    if (state_mutex) xSemaphoreGiveRecursive(state_mutex);
}

class StateLockGuard {
public:
    StateLockGuard() { state_lock(); }
    ~StateLockGuard() { state_unlock(); }
};

void state_init();
void state_save();
void state_save_theme();
void state_set_switch_locked(int idx, bool locked);
void state_set_wrelay_locked(int idx, bool locked);

#endif // SYSTEM_STATE_H
