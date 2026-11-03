#include "improv_serial.h"
#include "system_state.h"
#include "wifi_diagnostics.h"
#include "ui_main.h"
#include "debug_log.h"
#include <WiFi.h>

#define IMPROV_HEADER "IMPROV"
#define IMPROV_HEADER_LEN 6
#define IMPROV_VERSION 0x01

// Packet Types
#define IMPROV_TYPE_CURRENT_STATE 0x01
#define IMPROV_TYPE_ERROR_STATE   0x02
#define IMPROV_TYPE_RPC_COMMAND   0x03
#define IMPROV_TYPE_RPC_RESULT    0x04

// Current States
#define IMPROV_STATE_STOPPED      0x00
#define IMPROV_STATE_AWAIT_AUTH   0x01
#define IMPROV_STATE_READY        0x02
#define IMPROV_STATE_PROVISIONING 0x03
#define IMPROV_STATE_PROVISIONED  0x04

// Error States
#define IMPROV_ERR_NONE           0x00
#define IMPROV_ERR_INVALID_RPC    0x01
#define IMPROV_ERR_UNKNOWN_RPC    0x02
#define IMPROV_ERR_UNABLE_CONNECT 0x03
#define IMPROV_ERR_NOT_AUTHORIZED 0x04
#define IMPROV_ERR_UNKNOWN        0x05

// RPC Commands
#define IMPROV_CMD_SEND_WIFI      0x01
#define IMPROV_CMD_IDENTIFY       0x02
#define IMPROV_CMD_GET_INFO       0x03
#define IMPROV_CMD_SCAN_WIFI      0x04

static void send_raw_packet(uint8_t type, const uint8_t *data, uint8_t len) {
    uint8_t header[9] = {'I', 'M', 'P', 'R', 'O', 'V', IMPROV_VERSION, type, len};
    uint8_t checksum = 0;
    for (int i = 0; i < 9; i++) checksum += header[i];
    for (int i = 0; i < len; i++) checksum += data[i];

    CamperSerial.write(header, 9);
    if (len > 0 && data != nullptr) {
        CamperSerial.write(data, len);
    }
    CamperSerial.write(checksum);
    CamperSerial.write((uint8_t)'\n');
    CamperSerial.flush();
}

static void improv_send_state(uint8_t state_code) {
    uint8_t data[1] = { state_code };
    send_raw_packet(IMPROV_TYPE_CURRENT_STATE, data, 1);
}

static void improv_send_error(uint8_t error_code) {
    uint8_t data[1] = { error_code };
    send_raw_packet(IMPROV_TYPE_ERROR_STATE, data, 1);
}

static void improv_send_rpc_result(uint8_t command_id, const std::vector<String> &strings) {
    uint8_t payload[256];
    payload[0] = command_id;

    uint8_t total_str_len = 0;
    for (const auto &s : strings) {
        total_str_len += (1 + (uint8_t)s.length());
    }
    payload[1] = total_str_len;

    uint8_t offset = 2;
    for (const auto &s : strings) {
        uint8_t slen = (uint8_t)s.length();
        if (offset + 1 + slen > 250) break; // Avoid overflow
        payload[offset++] = slen;
        memcpy(&payload[offset], s.c_str(), slen);
        offset += slen;
    }

    send_raw_packet(IMPROV_TYPE_RPC_RESULT, payload, offset);
}

static void handle_rpc_command(uint8_t cmd_id, const uint8_t *data, uint8_t len) {
    switch (cmd_id) {
        case IMPROV_CMD_IDENTIFY: {
            // Identify / Request Current State
            if (WiFi.status() == WL_CONNECTED) {
                improv_send_state(IMPROV_STATE_PROVISIONED);
                std::vector<String> res;
                String url = "http://" + WiFi.localIP().toString() + "/settings";
                res.push_back(url);
                improv_send_rpc_result(IMPROV_CMD_IDENTIFY, res);
            } else {
                improv_send_state(IMPROV_STATE_READY);
                std::vector<String> res;
                improv_send_rpc_result(IMPROV_CMD_IDENTIFY, res);
            }
            break;
        }

        case IMPROV_CMD_GET_INFO: {
            // Request Device Info
            std::vector<String> info;
            info.push_back("CamperUI");          // Firmware name
            info.push_back(CAMPERUI_VERSION);   // Firmware version
            info.push_back("ESP32-S3");          // Chip family
            info.push_back("CamperUI Display");  // Device name
            improv_send_rpc_result(IMPROV_CMD_GET_INFO, info);
            break;
        }

        case IMPROV_CMD_SEND_WIFI: {
            // Send Wi-Fi Settings: format is [cmd_id, data_len, ssid_len, ssid..., pass_len, pass...]
            if (len < 3) {
                improv_send_error(IMPROV_ERR_INVALID_RPC);
                return;
            }
            uint8_t ssid_len = data[2];
            if (2 + ssid_len >= len) {
                improv_send_error(IMPROV_ERR_INVALID_RPC);
                return;
            }
            String ssid = "";
            for (uint8_t i = 0; i < ssid_len; i++) {
                ssid += (char)data[3 + i];
            }

            uint8_t pass_offset = 3 + ssid_len;
            uint8_t pass_len = data[pass_offset];
            String pass = "";
            for (uint8_t i = 0; i < pass_len && (pass_offset + 1 + i < len); i++) {
                pass += (char)data[pass_offset + 1 + i];
            }

            improv_send_state(IMPROV_STATE_PROVISIONING);
            {
                StateLockGuard lock;
                state.wifi_ssid = ssid;
                state.wifi_pass = pass;
            }
            state_save();
            wifi_connect_configured();
            break;
        }

        case IMPROV_CMD_SCAN_WIFI: {
            // Scanned networks request - return empty list
            std::vector<String> networks;
            improv_send_rpc_result(IMPROV_CMD_SCAN_WIFI, networks);
            break;
        }

        default:
            improv_send_error(IMPROV_ERR_UNKNOWN_RPC);
            break;
    }
}

// State Machine for Non-Blocking Serial Parsing
enum RxState {
    STATE_IDLE,
    STATE_HEADER,
    STATE_VERSION,
    STATE_TYPE,
    STATE_LEN,
    STATE_DATA,
    STATE_CHECKSUM
};

static RxState rx_state = STATE_IDLE;
static uint8_t header_pos = 0;
static uint8_t rx_type = 0;
static uint8_t rx_len = 0;
static uint8_t rx_data_pos = 0;
static uint8_t rx_data[256];
static uint8_t rx_full_packet[280];
static uint16_t rx_packet_pos = 0;

void improv_serial_init() {
    rx_state = STATE_IDLE;
    header_pos = 0;
    rx_packet_pos = 0;
}

bool improv_serial_process_char(char c) {
    uint8_t b = (uint8_t)c;

    switch (rx_state) {
        case STATE_IDLE:
            if (b == 'I') {
                header_pos = 1;
                rx_packet_pos = 0;
                rx_full_packet[rx_packet_pos++] = b;
                rx_state = STATE_HEADER;
                return true;
            }
            return false; // Regular character, pass to normal CLI handler

        case STATE_HEADER:
            if (b == IMPROV_HEADER[header_pos]) {
                rx_full_packet[rx_packet_pos++] = b;
                header_pos++;
                if (header_pos >= IMPROV_HEADER_LEN) {
                    rx_state = STATE_VERSION;
                }
                return true;
            } else {
                // Header mismatch: reset to idle
                rx_state = STATE_IDLE;
                header_pos = 0;
                return false;
            }

        case STATE_VERSION:
            rx_full_packet[rx_packet_pos++] = b;
            if (b == IMPROV_VERSION) {
                rx_state = STATE_TYPE;
            } else {
                rx_state = STATE_IDLE;
            }
            return true;

        case STATE_TYPE:
            rx_full_packet[rx_packet_pos++] = b;
            rx_type = b;
            rx_state = STATE_LEN;
            return true;

        case STATE_LEN:
            rx_full_packet[rx_packet_pos++] = b;
            rx_len = b;
            rx_data_pos = 0;
            if (rx_len == 0) {
                rx_state = STATE_CHECKSUM;
            } else {
                rx_state = STATE_DATA;
            }
            return true;

        case STATE_DATA:
            rx_full_packet[rx_packet_pos++] = b;
            if (rx_data_pos < sizeof(rx_data)) {
                rx_data[rx_data_pos] = b;
            }
            rx_data_pos++;
            if (rx_data_pos >= rx_len) {
                rx_state = STATE_CHECKSUM;
            }
            return true;

        case STATE_CHECKSUM: {
            uint8_t received_checksum = b;
            uint8_t calculated_checksum = 0;
            for (uint16_t i = 0; i < rx_packet_pos; i++) {
                calculated_checksum += rx_full_packet[i];
            }

            rx_state = STATE_IDLE;

            if (calculated_checksum == received_checksum) {
                if (rx_type == IMPROV_TYPE_RPC_COMMAND && rx_len > 0) {
                    uint8_t cmd_id = rx_data[0];
                    handle_rpc_command(cmd_id, rx_data, rx_len);
                }
            } else {
                improv_send_error(IMPROV_ERR_INVALID_RPC);
            }
            return true;
        }
    }

    return false;
}

void improv_serial_loop() {
    // Proactive background checks if needed
}

void improv_serial_on_wifi_connected() {
    improv_send_state(IMPROV_STATE_PROVISIONED);

    String ip = WiFi.localIP().toString();
    String settings_url = "http://" + ip + "/settings";

    std::vector<String> res;
    res.push_back(settings_url);

    // Send RPC result for both Send Wi-Fi (0x01) and Identify (0x02)
    improv_send_rpc_result(IMPROV_CMD_SEND_WIFI, res);
    improv_send_rpc_result(IMPROV_CMD_IDENTIFY, res);

    CamperSerial.printf("\n[WIFI] Verbunden mit: %s\n", WiFi.SSID().c_str());
    CamperSerial.printf("[WIFI] IP-Adresse:     %s\n", ip.c_str());
    CamperSerial.printf("[WIFI] Settings-URL:   %s\n", settings_url.c_str());
    CamperSerial.printf("[WIFI] mDNS-URL:       http://camperui.local/settings\n\n");
}

void improv_serial_on_wifi_disconnected() {
    improv_send_state(IMPROV_STATE_READY);
}
