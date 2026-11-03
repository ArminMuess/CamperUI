#include "web_ota.h"
#include "system_state.h"
#include "ui_main.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <HWCDC.h>
#include <Wire.h>
#include "WS_CH32_IO.h"

#include "debug_log.h"
#include "web_terminal_html.h"
#include "web_settings.h"
#include "ui_screensaver.h"

extern TwoWire Wire;

static void web_ota_restore_backlight() {
    uint8_t pwm = ui_screensaver_brightness_to_pwm(state.display_brightness);
    WS_CH32_IO::setPwm(Wire, pwm);
}

static WebServer server(80);
static bool server_started = false;
static bool mdns_started = false;

static bool is_updating = false;
static bool upload_finished = false;
static uint32_t restart_at = 0;
static int update_progress = 0;
static String update_status = "Bereit";

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CamperUI - Firmware Update</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: #14171d;
      color: #f8fafc;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 20px;
    }
    .card {
      background-color: #1e232b;
      border: 1px solid #2e3545;
      border-radius: 16px;
      padding: 32px;
      width: 100%;
      max-width: 460px;
      box-shadow: 0 10px 25px rgba(0,0,0,0.5);
    }
    .header {
      text-align: center;
      margin-bottom: 24px;
    }
    .badge {
      display: inline-block;
      background: rgba(14, 165, 233, 0.15);
      color: #0ea5e9;
      font-size: 13px;
      font-weight: 600;
      padding: 4px 12px;
      border-radius: 20px;
      margin-bottom: 12px;
      border: 1px solid rgba(14, 165, 233, 0.3);
    }
    h1 {
      font-size: 24px;
      font-weight: 700;
      margin-bottom: 6px;
    }
    p.sub {
      color: #94a3b8;
      font-size: 14px;
    }
    .drop-zone {
      border: 2px dashed #2e3545;
      border-radius: 12px;
      padding: 28px 16px;
      text-align: center;
      cursor: pointer;
      background: #14171d;
      transition: all 0.2s ease;
      margin-bottom: 20px;
    }
    .drop-zone:hover, .drop-zone.dragover {
      border-color: #0ea5e9;
      background: rgba(14, 165, 233, 0.05);
    }
    .drop-zone svg {
      width: 44px;
      height: 44px;
      fill: #0ea5e9;
      margin-bottom: 10px;
    }
    .drop-zone span {
      display: block;
      font-size: 14px;
      color: #cbd5e1;
    }
    .drop-zone small {
      display: block;
      color: #64748b;
      font-size: 12px;
      margin-top: 4px;
    }
    input[type="file"] { display: none; }
    .file-info {
      display: none;
      background: #181d24;
      border: 1px solid #2e3545;
      border-radius: 8px;
      padding: 10px 14px;
      font-size: 13px;
      color: #38bdf8;
      margin-bottom: 20px;
      word-break: break-all;
    }
    .btn {
      width: 100%;
      background: #0ea5e9;
      color: #fff;
      border: none;
      border-radius: 10px;
      padding: 14px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s;
    }
    .btn:hover { background: #0284c7; }
    .btn:disabled {
      background: #334155;
      color: #64748b;
      cursor: not-allowed;
    }
    .progress-box {
      display: none;
      margin-top: 20px;
    }
    .progress-bar-bg {
      background: #14171d;
      border-radius: 10px;
      height: 14px;
      overflow: hidden;
      margin-bottom: 8px;
      border: 1px solid #2e3545;
    }
    .progress-bar-fill {
      background: #10b981;
      height: 100%;
      width: 0%;
      transition: width 0.15s ease;
    }
    .progress-label {
      display: flex;
      justify-content: space-between;
      font-size: 13px;
      color: #94a3b8;
    }
    .status-msg {
      margin-top: 14px;
      text-align: center;
      font-size: 14px;
      color: #f1f5f9;
      min-height: 20px;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="header">
      <div class="badge" id="versionBadge">CamperUI</div>
      <h1>CamperUI Update</h1>
      <p class="sub">Neue Firmware (.bin) direkt auf den ESP32 laden</p>
      <div style="margin-top: 10px; display:flex; justify-content:center; gap:16px;">
        <a href="/settings" style="color: #0ea5e9; text-decoration: none; font-size: 13px; font-weight: 500;">&larr; Einstellungen</a>
        <a href="/status" style="color: #0ea5e9; text-decoration: none; font-size: 13px; font-weight: 500;">Terminal & Status</a>
      </div>
    </div>

    <div class="drop-zone" id="dropZone" onclick="document.getElementById('fileInput').click()">
      <svg viewBox="0 0 24 24"><path d="M19.35 10.04C18.67 6.59 15.64 4 12 4 9.11 4 6.6 5.64 5.35 8.04 2.34 8.36 0 10.91 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96zM14 13v4h-4v-4H7l5-5 5 5h-3z"/></svg>
      <span>Hier klicken oder .bin Datei hineinziehen</span>
      <small id="firmwareFileHint">Erstellt von build.bat</small>
    </div>

    <input type="file" id="fileInput" accept=".bin" onchange="onFileSelected()">
    <div class="file-info" id="fileInfo"></div>

    <button class="btn" id="btnUpload" onclick="startUpload()" disabled>Firmware installieren</button>

    <div class="progress-box" id="progressBox">
      <div class="progress-bar-bg">
        <div class="progress-bar-fill" id="progressFill"></div>
      </div>
      <div class="progress-label">
        <span id="progressPct">0%</span>
        <span id="progressBytes">0 / 0 MB</span>
      </div>
    </div>

    <div class="status-msg" id="statusMsg"></div>
  </div>

  <script>
    const fileInput = document.getElementById('fileInput');
    const fileInfo = document.getElementById('fileInfo');
    const btnUpload = document.getElementById('btnUpload');
    const dropZone = document.getElementById('dropZone');
    const progressBox = document.getElementById('progressBox');
    const progressFill = document.getElementById('progressFill');
    const progressPct = document.getElementById('progressPct');
    const progressBytes = document.getElementById('progressBytes');
    const statusMsg = document.getElementById('statusMsg');

    ['dragenter', 'dragover'].forEach(e => {
      dropZone.addEventListener(e, (ev) => { ev.preventDefault(); dropZone.classList.add('dragover'); });
    });
    ['dragleave', 'drop'].forEach(e => {
      dropZone.addEventListener(e, (ev) => { ev.preventDefault(); dropZone.classList.remove('dragover'); });
    });
    dropZone.addEventListener('drop', (ev) => {
      if (ev.dataTransfer.files.length > 0) {
        fileInput.files = ev.dataTransfer.files;
        onFileSelected();
      }
    });

    function onFileSelected() {
      if (fileInput.files.length > 0) {
        const file = fileInput.files[0];
        const sizeMb = (file.size / (1024 * 1024)).toFixed(2);
        fileInfo.style.display = 'block';
        fileInfo.textContent = 'Ausgewaehlt: ' + file.name + ' (' + sizeMb + ' MB)';
        btnUpload.disabled = false;
        statusMsg.textContent = '';
      }
    }

    function startUpload() {
      if (!fileInput.files.length) return;
      const file = fileInput.files[0];

      btnUpload.disabled = true;
      dropZone.style.pointerEvents = 'none';
      progressBox.style.display = 'block';
      statusMsg.textContent = 'Uebertrage Firmware an Display... Bitte warten.';

      const xhr = new XMLHttpRequest();
      const formData = new FormData();
      formData.append('update', file);

      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          progressFill.style.width = pct + '%';
          progressPct.textContent = pct + '%';
          const loadedMb = (e.loaded / (1024 * 1024)).toFixed(2);
          const totalMb = (e.total / (1024 * 1024)).toFixed(2);
          progressBytes.textContent = loadedMb + ' / ' + totalMb + ' MB';
          if (pct >= 100) {
            statusMsg.textContent = 'Flashe ESP32-S3 Flash-Speicher... Bitte nicht ausschalten!';
          }
        }
      });

      xhr.addEventListener('load', () => {
        if (xhr.status === 200) {
          progressFill.style.background = '#10b981';
          statusMsg.innerHTML = '<b style="color: #10b981;">Update erfolgreich!</b><br>Display startet in 5 Sekunden neu...';
          let count = 10;
          setInterval(() => {
            count--;
            if (count > 0) {
              statusMsg.innerHTML = '<b style="color: #10b981;">Update erfolgreich!</b><br>Neustart laeuft... Seite laedt neu in ' + count + 's';
            } else {
              window.location.reload();
            }
          }, 1000);
        } else {
          progressFill.style.background = '#ef4444';
          statusMsg.innerHTML = '<b style="color: #ef4444;">Fehler beim Flashen:</b> ' + (xhr.responseText || 'HTTP ' + xhr.status);
          btnUpload.disabled = false;
          dropZone.style.pointerEvents = 'auto';
        }
      });

      xhr.addEventListener('error', () => {
        progressFill.style.background = '#ef4444';
        statusMsg.innerHTML = '<b style="color: #ef4444;">Netzwerkfehler</b> waehrend der Uebertragung.';
        btnUpload.disabled = false;
        dropZone.style.pointerEvents = 'auto';
      });

      xhr.open('POST', '/update');
      xhr.send(formData);
    }

    fetch('/api/status').then(r => r.json()).then(d => {
      if (d.version) {
        document.getElementById('versionBadge').textContent = 'Version ' + d.version;
        document.getElementById('firmwareFileHint').textContent = 'Erstellt von build.bat (z. B. CamperUI-' + d.version + '.bin)';
      }
    }).catch(() => {});
  </script>
</body>
</html>
)rawliteral";

void web_ota_init() {
    state.web_ota_active = false;
    state.web_ota_progress = 0;
    state.web_ota_msg = "Bereit";
    is_updating = false;
    update_progress = 0;
    update_status = "Bereit";

    web_settings_register_routes(server);

    // Setup HTTP Endpoints
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Location", "/settings");
        server.send(303);
    });

    server.on("/update", HTTP_GET, []() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    auto send_status_json = []() {
        uint32_t uptime_sec = millis() / 1000;
        uint32_t h = uptime_sec / 3600;
        uint32_t m = (uptime_sec % 3600) / 60;
        uint32_t s = uptime_sec % 60;
        char uptime_buf[32];
        snprintf(uptime_buf, sizeof(uptime_buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);

        String json = "{";
        json += "\"version\":\"" + String(CAMPERUI_VERSION) + "\",";
        json += "\"uptime\":\"" + String(uptime_buf) + "\",";
        json += "\"heap_free\":" + String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) + ",";
        json += "\"heap_max\":" + String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)) + ",";
        json += "\"psram_free\":" + String(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + ",";
        json += "\"wifi_ssid\":\"" + state.wifi_ssid + "\",";
        json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"wifi_ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"vanpi_connected\":" + String((state.vanpi_connected && WiFi.status() == WL_CONNECTED) ? "true" : "false") + ",";
        json += "\"debug_mode\":" + String(state.debug_mode ? "true" : "false") + ",";
        json += "\"updating\":" + String(is_updating ? "true" : "false") + ",";
        json += "\"progress\":" + String(update_progress) + ",";
        json += "\"status\":\"" + update_status + "\"";
        json += "}";
        server.send(200, "application/json", json);
    };

    server.on("/api/status", HTTP_GET, send_status_json);

    server.on("/status", HTTP_GET, [send_status_json]() {
        if (server.hasArg("json") || (server.hasHeader("Accept") && server.header("Accept").indexOf("application/json") != -1 && server.header("Accept").indexOf("text/html") == -1)) {
            send_status_json();
        } else {
            server.send_P(200, "text/html", TERMINAL_HTML);
        }
    });

    server.on("/terminal", HTTP_GET, []() {
        server.send_P(200, "text/html", TERMINAL_HTML);
    });

    server.on("/api/log", HTTP_GET, []() {
        if (server.hasArg("clear") || server.arg("action") == "clear") {
            CamperSerial.clear();
            server.send(200, "text/plain", "Log geleert");
            return;
        }
        if (server.hasArg("download")) {
            server.sendHeader("Content-Disposition", "attachment; filename=\"camperui-debug.log\"");
        }
        CamperSerial.streamToHttp(server);
    });

    server.on("/api/command", HTTP_ANY, []() {
        String cmd = server.arg("cmd");
        if (cmd.length() > 0) {
            CamperSerial.injectInput(cmd.c_str());
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Fehlender 'cmd' Parameter");
        }
    });

    const char *headerkeys[] = {"Content-Length", "Accept"};
    server.collectHeaders(headerkeys, 2);

    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        if (Update.hasError() || !upload_finished) {
            is_updating = false; state.web_ota_active = false;
            web_ota_restore_backlight();
            server.send(500, "text/plain", upload_finished ? "Fehler: " + String(Update.errorString()) : "Upload nicht erfolgreich abgeschlossen");
        } else {
            server.send(200, "text/plain", "OK");
            // Finish the HTTP response before reboot; keep the old UI paused.
            restart_at = millis() + 1000;
        }
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("[WEB-OTA] Update Start: %s\n", upload.filename.c_str());
            WiFi.setSleep(false);
            is_updating = true;
            // Keep the panel lit. CH32 PWM survives an ESP-only reset.
            upload_finished = false; restart_at = 0;
            update_progress = 0;
            update_status = "Empfange Firmware...";
            state.web_ota_active = true;
            state.web_ota_progress = 0;
            state.web_ota_msg = "Lade Firmware...";

            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                Serial.printf("[WEB-OTA] Update.begin Fehler: %s\n", Update.errorString());
                Update.printError(Serial);
                update_status = "Fehler: " + String(Update.errorString());
                state.web_ota_msg = update_status;
                WiFi.setSleep(true);
                web_ota_restore_backlight();
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Serial.printf("[WEB-OTA] Update.write Fehler: %s\n", Update.errorString());
                Update.printError(Serial);
                update_status = "Schreibfehler!";
                state.web_ota_msg = update_status;
                WiFi.setSleep(true);
                web_ota_restore_backlight();
            } else {
                size_t total_len = server.header("Content-Length").toInt();
                if (total_len > 0) {
                    int pct = (int)((upload.totalSize * 100) / total_len);
                    update_progress = pct;
                    state.web_ota_progress = pct;
                    state.web_ota_msg = "Flashe: " + String(pct) + "%";
                }
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.printf("[WEB-OTA] Update erfolgreich! %u Bytes geflasht. Neustart...\n", upload.totalSize);
                upload_finished = true;
                update_progress = 100;
                update_status = "Erfolgreich! Neustart...";
                state.web_ota_progress = 100;
                state.web_ota_msg = "Update erfolgreich! Neustart...";
            } else {
                Serial.printf("[WEB-OTA] Update.end Fehler: %s\n", Update.errorString());
                Update.printError(Serial);
                is_updating = false;
                update_status = "Fehler beim Abschluss";
                state.web_ota_msg = update_status;
                WiFi.setSleep(true);
                web_ota_restore_backlight();
            }
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            Update.abort();
            Serial.println("[WEB-OTA] Update abgebrochen");
            is_updating = false;
            state.web_ota_active = false;
            update_status = "Abgebrochen";
            state.web_ota_msg = update_status;
            WiFi.setSleep(true);
            web_ota_restore_backlight();
        }
    });
}

void web_ota_loop() {
    if (restart_at && (int32_t)(millis() - restart_at) >= 0) {
        web_ota_restore_backlight();
        delay(50);
        ESP.restart();
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        if (!server_started) {
            server.begin();
            server_started = true;
            if (!mdns_started && MDNS.begin("camperui")) {
                MDNS.addService("http", "tcp", 80);
                mdns_started = true;
                Serial.printf("[WEB-OTA] Einstellungen:  http://%s/settings (oder http://camperui.local/settings)\n",
                              WiFi.localIP().toString().c_str());
                Serial.printf("[WEB-OTA] Web-Terminal:   http://%s/status (oder http://camperui.local/status)\n",
                              WiFi.localIP().toString().c_str());
                Serial.printf("[WEB-OTA] Firmware-Update: http://%s/update (oder http://camperui.local/update)\n",
                              WiFi.localIP().toString().c_str());
            } else {
                Serial.printf("[WEB-OTA] Settings: http://%s/settings | Status: http://%s/status | Update: http://%s/update\n",
                              WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
            }
        }
        server.handleClient();
    } else {
        if (server_started) {
            server.stop();
            server_started = false;
        }
    }
}

bool web_ota_is_updating() {
    return is_updating;
}

int web_ota_get_progress() {
    return update_progress;
}

const char* web_ota_get_status() {
    return update_status.c_str();
}

