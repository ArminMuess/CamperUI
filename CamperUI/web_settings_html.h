#ifndef WEB_SETTINGS_HTML_H
#define WEB_SETTINGS_HTML_H

#include <Arduino.h>

static const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>CamperUI - Einstellungen</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: #0b0e14;
      color: #f1f5f9;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      padding-bottom: 90px;
    }

    /* Header */
    header {
      background: #141822;
      border-bottom: 1px solid #232c3d;
      padding: 12px 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      position: sticky;
      top: 0;
      z-index: 100;
      backdrop-filter: blur(8px);
    }
    .header-left {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    h1 {
      font-size: 18px;
      font-weight: 700;
      color: #f8fafc;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .badge {
      font-size: 11px;
      font-weight: 600;
      padding: 2px 8px;
      border-radius: 12px;
      background: rgba(14, 165, 233, 0.15);
      color: #38bdf8;
      border: 1px solid rgba(14, 165, 233, 0.3);
    }
    .nav-links {
      display: flex;
      gap: 6px;
    }
    .nav-btn {
      background: #1e2430;
      color: #cbd5e1;
      text-decoration: none;
      font-size: 12px;
      font-weight: 600;
      padding: 7px 11px;
      border-radius: 8px;
      border: 1px solid #2c3649;
      display: inline-flex;
      align-items: center;
      gap: 4px;
      transition: all 0.2s;
    }
    .nav-btn:hover, .nav-btn.active {
      background: #0284c7;
      color: #fff;
      border-color: #38bdf8;
    }

    /* Horizontal Category Scroll (Mobile-First) */
    .cat-scroll-wrap {
      background: #10141d;
      border-bottom: 1px solid #1f2736;
      padding: 8px 12px;
      position: sticky;
      top: 53px;
      z-index: 90;
      overflow-x: auto;
      white-space: nowrap;
      -webkit-overflow-scrolling: touch;
      scrollbar-width: none;
    }
    .cat-scroll-wrap::-webkit-scrollbar { display: none; }
    .cat-pills {
      display: inline-flex;
      gap: 6px;
    }
    .cat-pill {
      background: #181f2b;
      color: #94a3b8;
      border: 1px solid #252f40;
      border-radius: 20px;
      padding: 6px 14px;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      transition: all 0.15s;
    }
    .cat-pill:hover {
      background: #222b3c;
      color: #f1f5f9;
    }
    .cat-pill.active {
      background: #0ea5e9;
      color: #fff;
      border-color: #38bdf8;
      box-shadow: 0 2px 8px rgba(14, 165, 233, 0.35);
    }

    /* Container & Cards */
    .container {
      max-width: 800px;
      width: 100%;
      margin: 0 auto;
      padding: 14px 14px 0 14px;
    }
    .tab-section {
      display: none;
      animation: fadeIn 0.2s ease-in-out;
    }
    .tab-section.active {
      display: block;
    }
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(4px); }
      to { opacity: 1; transform: translateY(0); }
    }

    .card {
      background: #141822;
      border: 1px solid #232c3d;
      border-radius: 14px;
      padding: 16px;
      margin-bottom: 14px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.25);
    }
    .card-title {
      font-size: 15px;
      font-weight: 700;
      color: #38bdf8;
      margin-bottom: 14px;
      display: flex;
      align-items: center;
      gap: 8px;
      border-bottom: 1px solid #1f2736;
      padding-bottom: 8px;
    }

    /* Form Fields */
    .field {
      margin-bottom: 14px;
    }
    .field:last-child {
      margin-bottom: 0;
    }
    .label {
      display: block;
      font-size: 13px;
      font-weight: 600;
      color: #cbd5e1;
      margin-bottom: 6px;
    }
    .subtext {
      font-size: 11px;
      color: #64748b;
      margin-top: 4px;
      line-height: 1.4;
    }

    input[type="text"], input[type="password"], input[type="number"], select {
      width: 100%;
      background: #0a0d13;
      border: 1px solid #2a3445;
      border-radius: 9px;
      padding: 11px 12px;
      color: #f8fafc;
      font-size: 15px;
      outline: none;
      transition: all 0.2s;
      appearance: none;
      -webkit-appearance: none;
    }
    select {
      background-image: url("data:image/svg+xml;charset=UTF-8,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%2394a3b8' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'%3e%3cpolyline points='6 9 12 15 18 9'%3e%3c/polyline%3e%3c/svg%3e");
      background-repeat: no-repeat;
      background-position: right 12px center;
      background-size: 16px;
      padding-right: 36px;
    }
    input:focus, select:focus {
      border-color: #0ea5e9;
      box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.2);
    }

    .input-row {
      display: flex;
      gap: 8px;
    }
    .btn-action {
      background: #1e2430;
      color: #38bdf8;
      border: 1px solid #2a3445;
      border-radius: 9px;
      padding: 0 14px;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      white-space: nowrap;
      transition: all 0.2s;
    }
    .btn-action:hover {
      background: #283344;
      border-color: #38bdf8;
      color: #fff;
    }
    .btn-theme-secondary {
      background: #1e2430;
      color: #cbd5e1;
      border: 1px solid #2a3445;
      border-radius: 10px;
      padding: 10px 16px;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      transition: all 0.2s ease;
    }
    .btn-theme-secondary:hover {
      background: #283344;
      border-color: #38bdf8;
      color: #fff;
    }
    .btn-theme-primary {
      background: #0ea5e9;
      color: #ffffff;
      border: none;
      border-radius: 10px;
      padding: 11px 18px;
      font-size: 14px;
      font-weight: 700;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      box-shadow: 0 4px 14px rgba(14, 165, 233, 0.35);
      transition: all 0.2s ease;
    }
    .btn-theme-primary:hover {
      background: #0284c7;
      box-shadow: 0 4px 18px rgba(14, 165, 233, 0.5);
    }
    .btn-theme-reset {
      background: #231b20;
      color: #f87171;
      border: 1px solid #3f2228;
      border-radius: 10px;
      padding: 10px 16px;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      transition: all 0.2s ease;
    }
    .btn-theme-reset:hover {
      background: #36222b;
      border-color: #ef4444;
      color: #fca5a5;
    }

    /* Grid & Two-Col */
    .grid-2 {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    @media (max-width: 480px) {
      .grid-2 {
        grid-template-columns: 1fr;
      }
    }

    /* Row with Toggle */
    .toggle-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 10px 0;
      border-bottom: 1px solid #1a202c;
    }
    .toggle-row:last-child {
      border-bottom: none;
      padding-bottom: 0;
    }
    .toggle-row:first-child {
      padding-top: 0;
    }
    .toggle-info {
      flex: 1;
      padding-right: 12px;
    }
    .toggle-title {
      font-size: 14px;
      font-weight: 600;
      color: #f1f5f9;
    }
    .toggle-desc {
      font-size: 12px;
      color: #64748b;
      margin-top: 2px;
    }

    /* Modern Switch */
    .switch {
      position: relative;
      display: inline-block;
      width: 48px;
      height: 28px;
      flex-shrink: 0;
    }
    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0; left: 0; right: 0; bottom: 0;
      background-color: #232c3d;
      transition: .2s;
      border-radius: 28px;
      border: 1px solid #323e54;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 22px;
      width: 22px;
      left: 2px;
      bottom: 2px;
      background-color: #cbd5e1;
      transition: .2s;
      border-radius: 50%;
      box-shadow: 0 2px 4px rgba(0,0,0,0.3);
    }
    input:checked + .slider {
      background-color: #10b981;
      border-color: #34d399;
    }
    input:checked + .slider:before {
      transform: translateX(20px);
      background-color: #ffffff;
    }

    /* Range Slider */
    .range-wrap {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-top: 6px;
    }
    input[type="range"] {
      flex: 1;
      height: 8px;
      border-radius: 4px;
      background: #1e2430;
      outline: none;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 24px;
      height: 24px;
      border-radius: 50%;
      background: #0ea5e9;
      cursor: pointer;
      border: 2px solid #ffffff;
      box-shadow: 0 2px 6px rgba(0,0,0,0.4);
    }
    .range-val {
      font-size: 14px;
      font-weight: 700;
      color: #38bdf8;
      min-width: 44px;
      text-align: right;
    }

    /* Tab Reorder List */
    .reorder-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .reorder-item {
      background: #0e1219;
      border: 1px solid #232c3d;
      border-radius: 10px;
      padding: 10px 12px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }
    .reorder-left {
      display: flex;
      align-items: center;
      gap: 10px;
      flex: 1;
    }
    .reorder-icon {
      font-size: 18px;
      width: 24px;
      text-align: center;
    }
    .reorder-name {
      font-size: 14px;
      font-weight: 600;
    }
    .reorder-btns {
      display: flex;
      gap: 4px;
    }
    .btn-arrow {
      background: #1a202c;
      color: #94a3b8;
      border: 1px solid #2c3649;
      border-radius: 6px;
      width: 32px;
      height: 32px;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      font-size: 14px;
      transition: all 0.15s;
    }
    .btn-arrow:hover:not(:disabled) {
      background: #0ea5e9;
      color: #fff;
    }
    .btn-arrow:disabled {
      opacity: 0.3;
      cursor: not-allowed;
    }

    /* Sticky Bottom Action Bar */
    .bottom-bar {
      position: fixed;
      bottom: 0;
      left: 0;
      right: 0;
      background: rgba(16, 20, 29, 0.92);
      backdrop-filter: blur(12px);
      border-top: 1px solid #232c3d;
      padding: 12px 16px;
      z-index: 100;
      display: flex;
      justify-content: center;
    }
    .bottom-bar-inner {
      max-width: 800px;
      width: 100%;
      display: flex;
      gap: 10px;
    }
    .btn-save {
      flex: 1;
      background: #10b981;
      color: #fff;
      border: none;
      border-radius: 12px;
      padding: 14px;
      font-size: 16px;
      font-weight: 700;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      box-shadow: 0 4px 14px rgba(16, 185, 129, 0.4);
      transition: all 0.2s;
    }
    .btn-save:hover {
      background: #059669;
    }
    .btn-save.dirty {
      background: #0ea5e9;
      box-shadow: 0 4px 14px rgba(14, 165, 233, 0.5);
    }
    .btn-reboot {
      background: #222938;
      color: #f87171;
      border: 1px solid #374151;
      border-radius: 12px;
      padding: 14px 16px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: 6px;
      white-space: nowrap;
      transition: all 0.2s;
    }
    .btn-reboot:hover {
      background: #7f1d1d;
      color: #fff;
    }

    /* Toast Notification */
    .toast {
      position: fixed;
      bottom: 84px;
      left: 50%;
      transform: translateX(-50%) translateY(20px);
      background: #10b981;
      color: white;
      padding: 10px 20px;
      border-radius: 30px;
      font-size: 14px;
      font-weight: 600;
      box-shadow: 0 8px 24px rgba(0,0,0,0.5);
      opacity: 0;
      visibility: hidden;
      transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
      z-index: 200;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .toast.show {
      opacity: 1;
      visibility: visible;
      transform: translateX(-50%) translateY(0);
    }
    .toast.error {
      background: #ef4444;
    }

    /* WiFi Scanner Modal */
    .modal {
      display: none;
      position: fixed;
      top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(0,0,0,0.7);
      z-index: 300;
      align-items: center;
      justify-content: center;
      padding: 16px;
    }
    .modal.show {
      display: flex;
    }
    .modal-box {
      background: #141822;
      border: 1px solid #232c3d;
      border-radius: 16px;
      max-width: 420px;
      width: 100%;
      max-height: 80vh;
      overflow-y: auto;
      padding: 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.6);
    }
    .modal-head {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 14px;
    }
    .modal-head h3 { font-size: 16px; color: #f8fafc; }
    .btn-close {
      background: none; border: none; color: #94a3b8; font-size: 20px; cursor: pointer;
    }
    .wifi-list { display: flex; flex-direction: column; gap: 8px; }
    .wifi-item {
      background: #0e1219;
      border: 1px solid #232c3d;
      border-radius: 10px;
      padding: 10px 14px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      cursor: pointer;
      transition: all 0.15s;
    }
    .wifi-item:hover {
      background: #1e2430;
      border-color: #0ea5e9;
    }
    .wifi-name { font-weight: 600; font-size: 14px; }
    .wifi-rssi { font-size: 12px; color: #94a3b8; }
  </style>
</head>
<body>

  <!-- Top Header -->
  <header>
    <div class="header-left">
      <h1>🚐 CamperUI</h1>
      <span class="badge" id="badgeVer">Lade...</span>
    </div>
    <div class="nav-links">
      <a href="/settings" class="nav-btn active">⚙️ Einstellungen</a>
      <a href="/status" class="nav-btn">📟 Terminal</a>
      <a href="/update" class="nav-btn">⚡ Update</a>
    </div>
  </header>

  <!-- Category Nav Pills (Scrollable Mobile-First) -->
  <div class="cat-scroll-wrap">
    <div class="cat-pills">
      <div class="cat-pill active" onclick="showTab('net')">🌐 Netzwerk</div>
      <div class="cat-pill" onclick="showTab('display')">📺 Display</div>
      <div class="cat-pill" onclick="showTab('theme')">&#127912; Theme &amp; Design</div>
      <div class="cat-pill" onclick="showTab('tabs')">📑 Menü & Tabs</div>
      <div class="cat-pill" onclick="showTab('water')">💧 Wassertanks</div>
      <div class="cat-pill" onclick="showTab('switches')">⚡ Schalter & Dimmer</div>
      <div class="cat-pill" onclick="showTab('sensors')">🌡️ Sensoren & Klima</div>
      <div class="cat-pill" onclick="showTab('home')">🏠 Home Kacheln</div>
      <div class="cat-pill" onclick="showTab('victron')">VE Victron GX</div>
    </div>
  </div>

  <div class="container">
    <form id="settingsForm" onsubmit="event.preventDefault(); saveSettings();">

      <!-- ================= TAB: NETZWERK ================= -->
      <div id="tab-net" class="tab-section active">
        <div class="card">
          <div class="card-title">🌐 WLAN & VanPi Gateway</div>
          <div class="field">
            <label class="label">WLAN-Name (SSID)</label>
            <div class="input-row">
              <input type="text" id="wifi_ssid" placeholder="WLAN Name eingeben">
              <button type="button" class="btn-action" onclick="scanWifi()">🔍 Scannen</button>
            </div>
          </div>
          <div class="field">
            <label class="label">WLAN-Passwort</label>
            <div class="input-row">
              <input type="password" id="wifi_pass" placeholder="Passwort eingeben">
              <button type="button" class="btn-action" onclick="togglePass()">👁️</button>
            </div>
          </div>
          <div class="field">
            <label class="label">VanPi Gateway IP</label>
            <input type="text" id="vanpi_ip" placeholder="192.168.1.xxx">
            <div class="subtext">IP des VanPi Systems für Port 1880 REST API</div>
          </div>
        </div>
      </div>

      <!-- ================= TAB: DISPLAY & HARDWARE ================= -->
      <div id="tab-display" class="tab-section">
        <div class="card">
          <div class="card-title">📺 Anzeige & Hardware</div>
          <div class="toggle-row">
            <div class="toggle-info">
              <div class="toggle-title">Dunkles Design (Dark Mode)</div>
              <div class="toggle-desc">Modernes OLED-Schwarz oder helles Tag-Design</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="dark_mode">
              <span class="slider"></span>
            </label>
          </div>
          <div class="toggle-row">
            <div class="toggle-info">
              <div class="toggle-title">Demo / Simulation</div>
              <div class="toggle-desc">Generiert Testdaten ohne VanPi-Verbindung</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="debug_mode">
              <span class="slider"></span>
            </label>
          </div>
        </div>

        <div class="card">
          <div class="card-title">💡 Helligkeit & Standby</div>
          <div class="field">
            <label class="label">Display Helligkeit</label>
            <div class="range-wrap">
              <input type="range" id="display_brightness" min="10" max="100" step="5" oninput="document.getElementById('val_bright').innerText = this.value + '%'">
              <span class="range-val" id="val_bright">80%</span>
            </div>
          </div>
          <div class="field">
            <label class="label">Display-Standby / Timeout</label>
            <select id="display_timeout">
              <option value="0">Immer an (Kein Standby)</option>
              <option value="15">15 Sekunden</option>
              <option value="30">30 Sekunden</option>
              <option value="60">1 Minute</option>
              <option value="120">2 Minuten</option>
              <option value="300">5 Minuten</option>
            </select>
          </div>
          <div class="toggle-row" style="margin-top:12px;">
            <div class="toggle-info">
              <div class="toggle-title">Screensaver</div>
              <div class="toggle-desc">Zeigt große Uhrzeit &amp; Restlaufzeit in Dunkelrot nach Timeout</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="screensaver_enabled">
              <span class="slider"></span>
            </label>
          </div>
          <div class="field" style="margin-top:12px;">
            <label class="label">Screensaver Helligkeit</label>
            <div class="range-wrap">
              <input type="range" id="screensaver_brightness" min="1" max="100" step="1" oninput="document.getElementById('val_scrsave_bright').innerText = this.value + '%'">
              <span class="range-val" id="val_scrsave_bright">5%</span>
            </div>
            <div class="subtext">Empfohlen: 1% bis 10% für angenehme Nachtanzeige</div>
          </div>
        </div>
      </div>

      <!-- ================= TAB: MENÜ & TABS ================= -->
      <!-- ================= TAB: THEME & DESIGN ================= -->
      <div id="tab-theme" class="tab-section">
        <div class="card">
          <div class="card-title">&#127912; Design &amp; Theme-Editor</div>
          <div class="subtext" style="margin-bottom:16px;">Passe Farben f&uuml;r Dimmer, Relais, F&uuml;llst&auml;nde, Kacheln und Rahmen direkt an dein Camper-Interieur an:</div>

          <!-- Live Preview -->
          <div style="background:#0b0d11; border-radius:12px; padding:16px; margin-bottom:20px; border:1px solid #1e2430;">
            <div style="font-size:11px; font-weight:700; text-transform:uppercase; color:#94a3b8; margin-bottom:12px; letter-spacing:0.8px;">&#128065; Live-Vorschau (CamperUI Display)</div>
            <div id="themePreviewApp" style="background:#101318; border-radius:12px; padding:14px; transition:background 0.2s ease;">
              <!-- Simulated Header -->
              <div id="themePreviewHeader" style="background:#181c24; border-bottom:1px solid #272d3b; padding:8px 12px; border-radius:10px 10px 0 0; display:flex; justify-content:space-between; align-items:center; margin-bottom:12px; transition:all 0.2s ease;">
                <span id="themePreviewHeaderText" style="color:#f1f5f9; font-weight:700; font-size:13px;">12:45 &nbsp; <small id="themePreviewHeaderSub" style="color:#94a3b8; font-weight:400;">v2.6.5</small></span>
                <span id="themePreviewHeaderBadge" style="background:#0ea5e9; color:#fff; font-size:10px; padding:2px 8px; border-radius:8px; font-weight:700;">13.4V &bull; 92%</span>
              </div>

              <!-- Simulated Card (Dimmer Slider) -->
              <div id="themePreviewCard" style="background:#181c24; border:1px solid #272d3b; border-radius:14px; padding:12px; margin-bottom:10px; transition:all 0.2s ease;">
                <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
                  <span id="themePreviewDimmerLabel" style="color:#f1f5f9; font-size:12px; font-weight:600;">Dimmer 1 (Wohnraum)</span>
                  <span id="themePreviewDimmerPct" style="color:#f59e0b; font-size:12px; font-weight:700;">75%</span>
                </div>
                <!-- Simulated Slider Track -->
                <div id="themePreviewTrack" style="background:#12151c; border:1px solid #272d3b; height:22px; border-radius:10px; position:relative; overflow:hidden;">
                  <div id="themePreviewDimmerBar" style="background:#f59e0b; width:75%; height:100%; border-radius:10px; transition:background 0.2s ease;"></div>
                </div>
              </div>

              <!-- Two column preview: Relay & Tank -->
              <div class="grid-2" style="margin-bottom:12px;">
                <div id="themePreviewRelayCard" style="background:#10b981; border:1px solid #10b981; border-radius:10px; padding:10px 12px; display:flex; justify-content:space-between; align-items:center; color:#fff; font-size:12px; font-weight:600; transition:all 0.2s ease;">
                  <span>Relais 1 (Pumpe)</span>
                  <span style="font-size:10px; background:rgba(0,0,0,0.25); padding:2px 6px; border-radius:6px;">EIN</span>
                </div>
                <div id="themePreviewTankCard" style="background:#181c24; border:1px solid #272d3b; border-radius:10px; padding:10px 12px; transition:all 0.2s ease;">
                  <div style="display:flex; justify-content:space-between; font-size:11px; margin-bottom:4px;">
                    <span id="themePreviewTankLabel" style="color:#94a3b8;">Frischwasser</span>
                    <span id="themePreviewTankPct" style="color:#f1f5f9; font-weight:700;">85%</span>
                  </div>
                  <div id="themePreviewTankTrack" style="background:#12151c; height:10px; border-radius:5px; overflow:hidden;">
                    <div id="themePreviewTankBar" style="background:#06b6d4; width:85%; height:100%; border-radius:5px; transition:all 0.2s ease;"></div>
                  </div>
                </div>
              </div>

              <!-- Action button preview (Primary Button like Tara / Settings) -->
              <button type="button" id="themePreviewBtn" style="background:#0ea5e9; color:#fff; border:none; border-radius:10px; padding:8px 16px; font-weight:700; font-size:12px; cursor:default; transition:all 0.2s ease;">&#128260; Nullpunkt Tara (Aktion)</button>
            </div>
          </div>

          <!-- Presets Dropdown -->
          <div class="field" style="margin-bottom:16px;">
            <label class="label">Theme-Vorlage (Preset)</label>
            <select id="theme_preset" class="select" onchange="applyThemePreset(this.value)">
              <option value="custom">-- Benutzerdefiniert (Custom) --</option>
              <option value="slate">Camper Slate (Standard Blau / Bernstein / Gr&uuml;n)</option>
              <option value="amber">Nordic Amber (Warmes Bernstein / Nacht-Orange)</option>
              <option value="forest">Forest Green (Moosgr&uuml;n &amp; Eiche)</option>
              <option value="ocean">Ocean Breeze (Marineblau &amp; Cyan)</option>
              <option value="minimal">Modern Minimal (Tiefschwarz &amp; Wei&szlig;, 0px Radius)</option>
              <option value="sunset">Sunset Terracotta (Warmgrau &amp; Terracotta)</option>
            </select>
          </div>

          <!-- 1. Color Pickers: Controls & Components -->
          <div style="font-size:14px; font-weight:600; color:#e2e8f0; margin-top:16px; margin-bottom:10px;">Komponenten- &amp; Akzentfarben</div>
          <div class="grid-2">
            <div class="field">
              <label class="label">Aktionen &amp; UI-Buttons (z.B. Tara, WLAN Scan)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_primary" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_primary', 'theme_hex_primary')">
                <input type="text" id="theme_hex_primary" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_primary', 'theme_col_primary')">
              </div>
            </div>

            <div class="field">
              <label class="label">Dimmer-Slider (Licht / Beleuchtung)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_dimmer" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_dimmer', 'theme_hex_dimmer')">
                <input type="text" id="theme_hex_dimmer" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_dimmer', 'theme_col_dimmer')">
              </div>
            </div>

            <div class="field">
              <label class="label">Schalter &amp; Relais (Aktiv-Zustand)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_relay" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_relay', 'theme_hex_relay')">
                <input type="text" id="theme_hex_relay" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_relay', 'theme_col_relay')">
              </div>
            </div>

            <div class="field">
              <label class="label">F&uuml;llst&auml;nde (Frischwasser &amp; Tanks)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_tank" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_tank', 'theme_hex_tank')">
                <input type="text" id="theme_hex_tank" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_tank', 'theme_col_tank')">
              </div>
            </div>
          </div>

          <!-- 2. Color Pickers: Surfaces & Backgrounds -->
          <div style="font-size:14px; font-weight:600; color:#e2e8f0; margin-top:20px; margin-bottom:10px;">Oberfl&auml;chen &amp; Hintergr&uuml;nde</div>
          <div class="grid-2">
            <div class="field">
              <label class="label">App-Hintergrund (Display BG)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_bg" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_bg', 'theme_hex_bg')">
                <input type="text" id="theme_hex_bg" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_bg', 'theme_col_bg')">
              </div>
            </div>

            <div class="field">
              <label class="label">Kachel- &amp; Header-Hintergrund (Card &amp; Bar BG)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_card" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_card', 'theme_hex_card')">
                <input type="text" id="theme_hex_card" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_card', 'theme_col_card')">
              </div>
            </div>

            <div class="field">
              <label class="label">Kanten- &amp; Rahmenfarbe (Border)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_border" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_border', 'theme_hex_border')">
                <input type="text" id="theme_hex_border" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_border', 'theme_col_border')">
              </div>
            </div>

            <div class="field">
              <label class="label">Regler-Schiene (Track BG f&uuml;r Slider &amp; Tanks)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_track" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_track', 'theme_hex_track')">
                <input type="text" id="theme_hex_track" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_track', 'theme_col_track')">
              </div>
            </div>
          </div>

          <!-- 3. Typography -->
          <div style="font-size:14px; font-weight:600; color:#e2e8f0; margin-top:20px; margin-bottom:10px;">Typografie &amp; Kontrast</div>
          <div class="grid-2">
            <div class="field">
              <label class="label">Haupttext (&Uuml;berschriften &amp; Werte)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_text" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_text', 'theme_hex_text')">
                <input type="text" id="theme_hex_text" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_text', 'theme_col_text')">
              </div>
            </div>

            <div class="field">
              <label class="label">Ged&auml;mpfter Text (Beschreibungen &amp; Icons)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_muted" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_muted', 'theme_hex_muted')">
                <input type="text" id="theme_hex_muted" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_muted', 'theme_col_muted')">
              </div>
            </div>

            <div class="field">
              <label class="label">Button-Text normal (Allgemeine GUI-Tasten)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_btn_text" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_btn_text', 'theme_hex_btn_text')">
                <input type="text" id="theme_hex_btn_text" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_btn_text', 'theme_col_btn_text')">
              </div>
            </div>

            <div class="field">
              <label class="label">Button-Text aktiv (Relais EIN, ausgew&auml;hlte Modi)</label>
              <div style="display:flex; gap:8px; align-items:center;">
                <input type="color" id="theme_col_btn_text_active" style="width:44px; height:38px; border:none; border-radius:6px; cursor:pointer; background:transparent;" oninput="syncColor('theme_col_btn_text_active', 'theme_hex_btn_text_active')">
                <input type="text" id="theme_hex_btn_text_active" class="input" style="font-family:monospace; text-transform:uppercase;" maxlength="7" oninput="syncHex('theme_hex_btn_text_active', 'theme_col_btn_text_active')">
              </div>
            </div>
          </div>

          <!-- Geometry / Radius Sliders -->
          <div style="font-size:14px; font-weight:600; color:#e2e8f0; margin-top:20px; margin-bottom:10px;">Form &amp; Rundungen</div>
          <div class="grid-2">
            <div class="field">
              <label class="label">Eckenrundung Kacheln (Cards)</label>
              <div class="range-wrap">
                <input type="range" id="theme_radius_card" min="0" max="24" step="1" oninput="document.getElementById('val_radius_card').innerText = this.value + ' px'; updateThemePreview();">
                <span class="range-val" id="val_radius_card">14 px</span>
              </div>
              <div class="subtext">0 = quadratisch / modern, 14 = Standard, 24 = stark abgerundet</div>
            </div>

            <div class="field">
              <label class="label">Eckenrundung Tasten &amp; Slider (Buttons)</label>
              <div class="range-wrap">
                <input type="range" id="theme_radius_button" min="0" max="18" step="1" oninput="document.getElementById('val_radius_button').innerText = this.value + ' px'; updateThemePreview();">
                <span class="range-val" id="val_radius_button">10 px</span>
              </div>
            </div>

            <div class="field">
              <label class="label">Rahmenst&auml;rke (Border Width)</label>
              <div class="range-wrap">
                <input type="range" id="theme_border_width" min="0" max="3" step="1" oninput="document.getElementById('val_border_width').innerText = this.value + ' px'; updateThemePreview();">
                <span class="range-val" id="val_border_width">1 px</span>
              </div>
            </div>
          </div>

          <!-- Theme Datei- & Reset-Aktionen -->
          <div style="margin-top:24px; padding-top:16px; border-top:1px solid #1e2430; display:flex; gap:10px; flex-wrap:wrap; align-items:center;">
            <button type="button" class="btn-theme-secondary" onclick="exportThemeFile()">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
              Theme herunterladen (.json)
            </button>
            <button type="button" class="btn-theme-secondary" onclick="document.getElementById('theme_file_input').click()">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
              Theme hochladen (.json)
            </button>
            <button type="button" class="btn-theme-reset" onclick="resetThemeToDefault()">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8"/><path d="M3 3v5h5"/></svg>
              Auf Werkseinstellungen zur&uuml;cksetzen
            </button>
            <input type="file" id="theme_file_input" accept=".json" style="display:none;" onchange="importThemeFile(this)">
          </div>
        </div>
      </div>

      <div id="tab-tabs" class="tab-section">
        <div class="card">
          <div class="card-title">📑 Menüleiste anpassen</div>
          <div class="subtext" style="margin-bottom:12px;">Aktiviere oder deaktiviere Tabs und ändere die Reihenfolge in der unteren Navigationsleiste:<br><small style="color:#94a3b8; display:inline-block; margin-top:5px;">💡 Notfall-Zugriff: Wenn Settings ausgeblendet ist, halte 5 Sekunden lang irgendwo die untere Tableiste gedrückt.</small></div>
          <div class="reorder-list" id="tabReorderList">
            <!-- Dynamisch befüllt -->
          </div>
        </div>
      </div>

      <!-- ================= TAB: WASSER ================= -->
      <div id="tab-water" class="tab-section">
        <div class="card">
          <div class="card-title">💧 Wassertanks Konfiguration</div>
          <div class="field">
            <label class="label">Anzahl sichtbare Tanks</label>
            <select id="tank_count" onchange="updateTankInputs()">
              <option value="1">1 Tank</option>
              <option value="2">2 Tanks</option>
              <option value="3">3 Tanks</option>
              <option value="4">4 Tanks</option>
            </select>
          </div>
          <div class="toggle-row" style="margin-bottom:12px;">
            <div class="toggle-info">
              <div class="toggle-title">Einheit/Menge unter Prozent anzeigen</div>
              <div class="toggle-desc">Berechnet Restmenge (L / kg) anhand der Kapazität</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="tank_show_liters">
              <span class="slider"></span>
            </label>
          </div>
          <div class="grid-2">
            <div class="field" id="wrap_tank0">
              <label class="label">Kapazität &amp; Einheit Tank 1</label>
              <div style="display:flex; gap:8px;">
                <input type="number" id="tank_max_0" min="10" max="1000" style="flex:1;">
                <select id="tank_unit_0" style="width:75px;">
                  <option value="0">L</option>
                  <option value="1">kg</option>
                </select>
              </div>
            </div>
            <div class="field" id="wrap_tank1">
              <label class="label">Kapazität &amp; Einheit Tank 2</label>
              <div style="display:flex; gap:8px;">
                <input type="number" id="tank_max_1" min="10" max="1000" style="flex:1;">
                <select id="tank_unit_1" style="width:75px;">
                  <option value="0">L</option>
                  <option value="1">kg</option>
                </select>
              </div>
            </div>
            <div class="field" id="wrap_tank2">
              <label class="label">Kapazität &amp; Einheit Tank 3</label>
              <div style="display:flex; gap:8px;">
                <input type="number" id="tank_max_2" min="10" max="1000" style="flex:1;">
                <select id="tank_unit_2" style="width:75px;">
                  <option value="0">L</option>
                  <option value="1">kg</option>
                </select>
              </div>
            </div>
            <div class="field" id="wrap_tank3">
              <label class="label">Kapazität &amp; Einheit Tank 4</label>
              <div style="display:flex; gap:8px;">
                <input type="number" id="tank_max_3" min="10" max="1000" style="flex:1;">
                <select id="tank_unit_3" style="width:75px;">
                  <option value="0">L</option>
                  <option value="1">kg</option>
                </select>
              </div>
            </div>
          </div>
        </div>

        <div class="card">
          <div class="card-title">🚰 Pumpen- und Ablaufsteuerung</div>
          <div class="grid-2">
            <div class="field">
              <label class="label">Frischwasser-Pumpe Relais</label>
              <select id="pump_relay"></select>
            </div>
            <div class="field">
              <label class="label">Abwasserventil Relais</label>
              <select id="drain_relay"></select>
            </div>
          </div>
        </div>
      </div>

      <!-- ================= TAB: SCHALTER & DIMMER ================= -->
      <div id="tab-switches" class="tab-section">
        <div class="card">
          <div class="card-title">🔘 Relais (Haupt- &amp; Erweiterungsmodule)</div>
          <div class="toggle-row" style="margin-bottom:12px;">
            <div class="toggle-info">
              <div class="toggle-title">out-x-in Pro aktivieren</div>
              <div class="toggle-desc">Schaltet Relais 9–16 frei</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="show_relay_ext" onchange="toggleRelayExt(this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div id="relaysList"></div>
        </div>

        <div class="card">
          <div class="card-title">💡 Dimmer Hardware &amp; Sichtbarkeit</div>
          <div class="toggle-row" style="margin-bottom:12px;">
            <div class="toggle-info">
              <div class="toggle-title">Dimmy Pro Modus</div>
              <div class="toggle-desc">Kanäle 8–15 + 2x RGBW freischalten</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="dimmy_pro" onchange="toggleDimmyPro(this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          <div id="dimmersList"></div>
        </div>

        <div class="card">
          <div class="card-title">📶 WiFi-Relais (Zusatzrelais)</div>
          <div class="toggle-row" style="margin-bottom:12px;">
            <div class="toggle-info">
              <div class="toggle-title">WiFi-Relais aktivieren</div>
              <div class="toggle-desc">Blendet WLAN-Schalter ein</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="show_wrelay" onchange="document.getElementById('wrelaysList').style.display = this.checked ? 'block' : 'none'">
              <span class="slider"></span>
            </label>
          </div>
          <div id="wrelaysList"></div>
        </div>
      </div>

      <!-- ================= TAB: SENSOREN & ALARME ================= -->
      <div id="tab-sensors" class="tab-section">
        <div class="card">
          <div class="card-title">🔥 Heizung &amp; Klima</div>
          <div class="field">
            <label class="label">Verbaute Standheizung</label>
            <select id="heater_type"></select>
          </div>
          <div class="subtext" style="margin-top:6px; color:#94a3b8; font-size:12px;">Wähle das verbaute Heizungssystem (z.B. Autoterm, Truma).</div>
          <div class="toggle-row" style="margin-top:12px;">
            <div class="toggle-info">
              <div class="toggle-title">Temperaturtasten (+ / -) am Arc</div>
              <div class="toggle-desc">Runde Tasten neben dem Temperaturregler zum präzisen Einstellen</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="heater_temp_buttons">
              <span class="slider"></span>
            </label>
          </div>
        </div>

        <div class="card">
          <div class="card-title">🌡️ Sensor Zuordnung</div>
          <div class="field">
            <label class="label">Außentemperatur-Sensor</label>
            <select id="outdoor_temp_sensor"></select>
          </div>
          <div class="field">
            <label class="label">Innentemperatur-Sensor</label>
            <select id="indoor_temp_sensor"></select>
          </div>
          <div class="field">
            <label class="label">Kühlschranktemperatur-Sensor</label>
            <select id="fridge_temp_sensor"></select>
          </div>
          <div class="grid-2">
            <div class="field">
              <label class="label">Zusatz-Sensor 1</label>
              <select id="temps_custom_sensor_0"></select>
            </div>
            <div class="field">
              <label class="label">Zusatz-Sensor 2</label>
              <select id="temps_custom_sensor_1"></select>
            </div>
          </div>
        </div>

        <div class="card">
          <div class="card-title">🚨 Smart Header Alarmschwellen</div>
          <div class="grid-2">
            <div class="field">
              <label class="label">Frostgrenze Außen (&le; &deg;C)</label>
              <input type="number" step="0.5" id="warn_frost_temp">
            </div>
            <div class="field">
              <label class="label">Batterie Rot (&le; % SoC)</label>
              <input type="number" id="warn_bat_soc" min="0" max="100">
            </div>
            <div class="field">
              <label class="label">Batterie Gelb (&le; % SoC)</label>
              <input type="number" id="warn_bat_yellow_soc" min="0" max="100">
            </div>
            <div class="field">
              <label class="label">Frischwasser Alarm (&le; %)</label>
              <input type="number" id="warn_fresh_min" min="0" max="100">
            </div>
            <div class="field">
              <label class="label">Abwasser Alarm (&ge; %)</label>
              <input type="number" id="warn_waste_max" min="0" max="100">
            </div>
          </div>
        </div>

        <div class="card">
          <div class="card-title">🔋 Batterie &amp; Energie Dashboard</div>
          <div class="toggle-row">
            <div class="toggle-info">
              <div class="toggle-title">Solar-Erzeuger anzeigen</div>
              <div class="toggle-desc">Zeigt Solar-Kachel &amp; Leistungs-Arc im Energie-Tab</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="power_show_solar">
              <span class="slider"></span>
            </label>
          </div>
          <div class="toggle-row">
            <div class="toggle-info">
              <div class="toggle-title">Landstrom anzeigen (230V)</div>
              <div class="toggle-desc">Zeigt Landstrom-Kachel &amp; Leistungs-Arc im Energie-Tab</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="power_show_shore">
              <span class="slider"></span>
            </label>
          </div>
          <div class="toggle-row">
            <div class="toggle-info">
              <div class="toggle-title">Lichtmaschine / Booster anzeigen</div>
              <div class="toggle-desc">Zeigt Lima/B2B-Kachel &amp; Leistungs-Arc im Energie-Tab</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="power_show_alt">
              <span class="slider"></span>
            </label>
          </div>
          <div class="toggle-row">
            <div class="toggle-info">
              <div class="toggle-title">Verbrauch mit Erzeugern verrechnen</div>
              <div class="toggle-desc">Aktiv: Berechnet Gesamtlast (Erzeuger − Batterie). Inaktiv: Nur reine Entnahme aus /batt.</div>
            </div>
            <label class="switch">
              <input type="checkbox" id="power_calc_load">
              <span class="slider"></span>
            </label>
          </div>
          <div class="grid-2" style="margin-top:14px;">
            <div class="field">
              <label class="label">Solar Maximum (W)</label>
              <input type="number" id="solar_max_w">
            </div>
            <div class="field">
              <label class="label">Landstrom Maximum (W)</label>
              <input type="number" id="shore_max_w">
            </div>
            <div class="field">
              <label class="label">Lichtmaschine / Booster Max (W)</label>
              <input type="number" id="alt_max_w">
            </div>
            <div class="field">
              <label class="label">Batterie Kapazität (Ah)</label>
              <input type="number" id="bat_capacity_ah">
            </div>
          </div>
        </div>
      </div>

      <!-- ================= TAB: HOME KACHELN ================= -->
      <div id="tab-home" class="tab-section">
        <div class="card">
          <div class="card-title">🏠 Home-Screen Widgets</div>
          <div class="toggle-row">
            <div class="toggle-info"><div class="toggle-title">Batterie-Kachel</div></div>
            <label class="switch"><input type="checkbox" id="home_show_battery"><span class="slider"></span></label>
          </div>
          <div class="toggle-row">
            <div class="toggle-info"><div class="toggle-title">Starter-Batterie</div></div>
            <label class="switch"><input type="checkbox" id="home_show_starter"><span class="slider"></span></label>
          </div>
          <div class="toggle-row">
            <div class="toggle-info"><div class="toggle-title">Solar-Kachel</div></div>
            <label class="switch"><input type="checkbox" id="home_show_solar"><span class="slider"></span></label>
          </div>
          <div class="toggle-row">
            <div class="toggle-info"><div class="toggle-title">Wasser-Kachel</div></div>
            <label class="switch"><input type="checkbox" id="home_show_water" onchange="homeEnergyChoice(false)"><span class="slider"></span></label>
          </div>
          <div class="toggle-row"><div class="toggle-info"><div class="toggle-title">MultiPlus-Kachel statt Wasser</div></div>
            <label class="switch"><input type="checkbox" id="home_show_victron" onchange="homeEnergyChoice(true)"><span class="slider"></span></label></div>
          <div class="field" style="margin-top:12px;">
            <label class="label">Wasser-Quelle auf Home</label>
            <select id="home_water_source">
              <option value="0">Tank 1 (Frischwasser)</option>
              <option value="1">Tank 2 (Grauwasser)</option>
              <option value="2">Tank 3</option>
              <option value="3">Tank 4</option>
            </select>
          </div>
          <div class="grid-2" style="margin-top:12px;">
            <div class="field">
              <label class="label">Favoriten-Schalter 1</label>
              <select id="home_favorite_0"></select>
            </div>
            <div class="field">
              <label class="label">Favoriten-Schalter 2</label>
              <select id="home_favorite_1"></select>
            </div>
          </div>
        </div>
      </div>

      <div id="tab-victron" class="tab-section">
        <div class="card">
          <div class="card-title">VE Victron GX / MultiPlus-II</div>
          <div class="toggle-row"><div class="toggle-info"><div class="toggle-title">MQTT-Anbindung</div></div>
            <label class="switch"><input type="checkbox" id="victron_enabled"><span class="slider"></span></label></div>
          <div class="toggle-row"><div class="toggle-info"><div class="toggle-title">Eigener Victron-Keepalive</div>
            <div class="subtext">Aus lassen, wenn VanPi ihn bereits übernimmt.</div></div>
            <label class="switch"><input type="checkbox" id="victron_keepalive"><span class="slider"></span></label></div>
          <div class="field"><label class="label">Quelle der Starterbatterie-Spannung</label>
            <select id="victron_starter"><option value="0">VanPi (bisherige Quelle)</option><option value="1">Cerbo GX – SmartShunt/BMV</option></select></div>
          <div class="field"><label class="label">Starter: Batteriemonitor-Geräte-ID</label>
            <input id="victron_battery_instance" type="number" min="0" max="65535" placeholder="Automatisch">
            <div class="subtext">Starter-Eingang am Batteriemonitor erforderlich. Bei mehreren Monitoren die passende ID wählen. Fehlende GX-Werte erscheinen als --.</div></div>
          <div class="field"><label class="label">Cerbo-IP / Hostname</label>
            <input type="text" id="victron_host" maxlength="63" placeholder="z. B. 192.168.1.50" autocomplete="off"></div>
          <div class="grid-2"><div class="field"><label class="label">MQTT-Port (TCP, ohne TLS)</label>
            <input id="victron_port" type="number" min="1" max="65535" value="1883"></div>
            <div class="field"><label class="label">MultiPlus: VRM-/VE.Bus-Instanz</label>
            <input id="victron_instance" type="number" min="0" max="65535" placeholder="Automatisch"></div></div>
          <div class="field"><label class="label">Portal-ID</label>
            <input type="text" id="victron_portal" maxlength="32" placeholder="Automatisch" autocomplete="off"></div>
          <div class="subtext" id="victron_status" style="white-space:pre-line;margin-bottom:12px;">Status wird geladen…</div>
          <div class="grid-2"><div class="field"><label class="label">Relais 1: Anzeigename</label>
            <input type="text" id="victron_relay_0" oninput="refreshGxFavorites()" maxlength="32"></div><div class="field"><label class="label">Relais 2: Anzeigename</label>
            <input type="text" id="victron_relay_1" oninput="refreshGxFavorites()" maxlength="32"></div></div>
          <div class="grid-2"><div class="field"><label class="label">MQTT-Benutzer (optional)</label>
            <input type="text" id="victron_user" maxlength="64" autocomplete="off"></div>
            <div class="field"><label class="label">MQTT-Passwort (optional)</label>
            <input id="victron_password" type="password" maxlength="64" autocomplete="new-password"></div></div>
          <label class="subtext"><input type="checkbox" id="victron_clear_password"> Gespeichertes MQTT-Passwort löschen</label>
          <div class="subtext" style="margin-top:12px;">MultiPlus-ID: numerische VRM-Instanz, keine Product-ID wie 0x2605. GX-Relais am Cerbo auf „Manuell“ stellen. Den Victron-Tab unter „Menü &amp; Tabs“ aktivieren.
            Ohne Daten kann eine manuelle Portal-ID nötig sein. Im Dummy-Modus werden keine MQTT-Schaltbefehle gesendet.</div>
        </div>
      </div>

    </form>
  </div>

  <!-- Sticky Bottom Save Bar -->
  <div class="bottom-bar">
    <div class="bottom-bar-inner">
      <button type="button" class="btn-save" id="btnSave" onclick="saveSettings()">
        <span>💾</span> Einstellungen speichern
      </button>
      <button type="button" class="btn-reboot" onclick="rebootEsp()">
        <span>🔄</span> Neustart
      </button>
    </div>
  </div>

  <!-- Toast -->
  <div class="toast" id="toast">✅ Gespeichert!</div>

  <!-- WiFi Scanner Modal -->
  <div class="modal" id="wifiModal">
    <div class="modal-box">
      <div class="modal-head">
        <h3>Gefundene WLAN-Netzwerke</h3>
        <button type="button" class="btn-close" onclick="closeWifiModal()">&times;</button>
      </div>
      <div class="wifi-list" id="wifiList">Scanne...</div>
    </div>
  </div>

  <script>
    let currentSettings = {};
    let tabOrder = [];
    let tabEnabled = {};
    const tabMeta = [
      { id: 0, name: "Home", icon: "🏠" },
      { id: 1, name: "Dimmer", icon: "💡" },
      { id: 2, name: "Power", icon: "🔋" },
      { id: 3, name: "Wasser", icon: "💧" },
      { id: 4, name: "Klima", icon: "🔥" },
      { id: 5, name: "Temps", icon: "🌡️" },
      { id: 6, name: "MaxxFan", icon: "💨" },
      { id: 7, name: "Schalter", icon: "🔘" },
      { id: 8, name: "Level", icon: "⚖️" },
      { id: 9, name: "Einstellungen", icon: "⚙️" },
      { id: 10, name: "Truma", icon: "🔥" },
      { id: 11, name: "Victron", icon: "VE" },
      { id: 12, name: "Kühlschrank", icon: "❄️" },
      { id: 13, name: "Radio", icon: "📻" }
    ];

    function showTab(id) {
      document.querySelectorAll('.tab-section').forEach(el => el.classList.remove('active'));
      document.querySelectorAll('.cat-pill').forEach(el => el.classList.remove('active'));
      const sec = document.getElementById('tab-' + id);
      if (sec) sec.classList.add('active');
      if (window.event && window.event.currentTarget && window.event.currentTarget.classList) {
        window.event.currentTarget.classList.add('active');
      }
      window.scrollTo({ top: 0, behavior: 'smooth' });
      if (id === 'victron') pollVictronStatus();

      const btnSave = document.getElementById('btnSave');
      if (btnSave) {
        if (id === 'theme') {
          btnSave.innerHTML = '<span>💾</span> Theme speichern &amp; anwenden';
          btnSave.onclick = saveAndApplyTheme;
        } else {
          btnSave.innerHTML = '<span>💾</span> Einstellungen speichern';
          btnSave.onclick = saveSettings;
        }
      }
    }

    function togglePass() {
      const p = document.getElementById('wifi_pass');
      p.type = p.type === 'password' ? 'text' : 'password';
    }

    function showToast(msg, isError = false) {
      const t = document.getElementById('toast');
      t.innerText = msg;
      t.className = 'toast show' + (isError ? ' error' : '');
      setTimeout(() => { t.className = 'toast'; }, 3000);
    }

    function loadSettings() {
      fetch('/api/settings')
        .then(r => r.json())
        .then(data => {
          document.getElementById('badgeVer').innerText = data.version || 'V1.9.1';
          
          // Network
          document.getElementById('wifi_ssid').value = data.wifi_ssid || '';
          document.getElementById('wifi_pass').value = data.wifi_pass || '';
          document.getElementById('vanpi_ip').value = data.vanpi_ip || '';

          // Display
          document.getElementById('dark_mode').checked = !!data.dark_mode;
          document.getElementById('debug_mode').checked = !!data.debug_mode;
          document.getElementById('display_brightness').value = data.display_brightness || 80;
          document.getElementById('val_bright').innerText = (data.display_brightness || 80) + '%';
          document.getElementById('display_timeout').value = data.display_timeout !== undefined ? data.display_timeout : 60;
          document.getElementById('screensaver_enabled').checked = !!data.screensaver_enabled;
          document.getElementById('screensaver_brightness').value = data.screensaver_brightness !== undefined ? data.screensaver_brightness : 5;
          document.getElementById('val_scrsave_bright').innerText = (data.screensaver_brightness !== undefined ? data.screensaver_brightness : 5) + '%';

          // Water
          document.getElementById('tank_count').value = data.tank_count || 1;
          document.getElementById('tank_show_liters').checked = !!data.tank_show_liters;
          for (let i = 0; i < 4; i++) {
            const el = document.getElementById('tank_max_' + i);
            if (el) el.value = (data.tank_max && data.tank_max[i]) ? data.tank_max[i] : 100;
            const elU = document.getElementById('tank_unit_' + i);
            if (elU) elU.value = (data.tank_unit && data.tank_unit[i] !== undefined) ? data.tank_unit[i] : 0;
          }
          updateTankInputs();

          // Dropdowns: Relays
          populateRelaySelect('pump_relay', data.relays, data.pump_relay);
          populateRelaySelect('drain_relay', data.relays, data.drain_relay);

          // Heater
          populateHeaterTypes(data.heater_types, data.heater_type !== undefined ? data.heater_type : 0);
          document.getElementById('heater_temp_buttons').checked = !!data.heater_temp_buttons;

          // Relays & Dimmers lists
          document.getElementById('show_relay_ext').checked = !!data.show_relay_ext;
          document.getElementById('dimmy_pro').checked = !!data.dimmy_pro;
          buildRelaysList(data.switch_names, data.switch_visible, !!data.show_relay_ext);
          buildDimmersList(data.dimmer_names, data.dimmer_visible, !!data.dimmy_pro);
          document.getElementById('show_wrelay').checked = !!data.show_wrelay;
          document.getElementById('wrelaysList').style.display = data.show_wrelay ? 'block' : 'none';
          buildWRelaysList(data.wrelay_names, data.wrelay_visible);

          // Sensors
          populateTempSensorsSelect('outdoor_temp_sensor', data.temp_sensors, data.outdoor_temp_sensor, false);
          populateTempSensorsSelect('indoor_temp_sensor', data.temp_sensors, data.indoor_temp_sensor, false);
          populateTempSensorsSelect('fridge_temp_sensor', data.temp_sensors, data.fridge_temp_sensor !== undefined ? data.fridge_temp_sensor : -1, true);
          populateTempSensorsSelect('temps_custom_sensor_0', data.temp_sensors, (data.temps_custom_sensor ? data.temps_custom_sensor[0] : -1), true);
          populateTempSensorsSelect('temps_custom_sensor_1', data.temp_sensors, (data.temps_custom_sensor ? data.temps_custom_sensor[1] : -1), true);

          // Alarms
          document.getElementById('warn_frost_temp').value = data.warn_frost_temp !== undefined ? data.warn_frost_temp : 3.0;
          document.getElementById('warn_bat_soc').value = data.warn_bat_soc !== undefined ? data.warn_bat_soc : 15;
          document.getElementById('warn_bat_yellow_soc').value = data.warn_bat_yellow_soc !== undefined ? data.warn_bat_yellow_soc : 30;
          document.getElementById('warn_fresh_min').value = data.warn_fresh_min !== undefined ? data.warn_fresh_min : 15;
          document.getElementById('warn_waste_max').value = data.warn_waste_max !== undefined ? data.warn_waste_max : 85;
          document.getElementById('power_show_solar').checked = data.power_show_solar !== undefined ? !!data.power_show_solar : true;

          const shoreAvail = data.shore_available !== undefined ? !!data.shore_available : true;
          const altAvail = data.alt_available !== undefined ? !!data.alt_available : true;

          const elShore = document.getElementById('power_show_shore');
          elShore.checked = shoreAvail ? (data.power_show_shore !== undefined ? !!data.power_show_shore : true) : false;
          elShore.disabled = !shoreAvail;
          elShore.closest('.toggle-row').style.opacity = shoreAvail ? '1' : '0.4';

          const elAlt = document.getElementById('power_show_alt');
          elAlt.checked = altAvail ? (data.power_show_alt !== undefined ? !!data.power_show_alt : true) : false;
          elAlt.disabled = !altAvail;
          elAlt.closest('.toggle-row').style.opacity = altAvail ? '1' : '0.4';
          document.getElementById('power_calc_load').checked = data.power_calc_load !== undefined ? !!data.power_calc_load : false;
          document.getElementById('solar_max_w').value = data.solar_max_w || 200;
          document.getElementById('shore_max_w').value = data.shore_max_w || 2000;
          document.getElementById('alt_max_w').value = data.alt_max_w || 600;
          document.getElementById('bat_capacity_ah').value = data.bat_capacity_ah || 100;

          // Home Widgets
          document.getElementById('home_show_battery').checked = !!data.home_show_battery;
          document.getElementById('home_show_starter').checked = !!data.home_show_starter;
          document.getElementById('home_show_solar').checked = !!data.home_show_solar;
          document.getElementById('home_show_water').checked = !!data.home_show_water;
          document.getElementById('home_show_victron').checked = !!data.home_show_victron;
          document.getElementById('victron_enabled').checked = !!data.victron_enabled;
          document.getElementById('victron_keepalive').checked = !!data.victron_keepalive;
          document.getElementById('victron_starter').value = data.victron_starter ? '1' : '0';
          document.getElementById('victron_battery_instance').value = data.victron_battery_instance >= 0 ? data.victron_battery_instance : '';
          for(const key of ['victron_host','victron_portal','victron_user'])document.getElementById(key).value = data[key] || '';
          document.getElementById('victron_port').value = data.victron_port || 1883;
          document.getElementById('victron_instance').value = data.victron_instance >= 0 ? data.victron_instance : '';
          document.getElementById('victron_password').value = '';
          document.getElementById('victron_password').placeholder = data.victron_password_set ? 'Gespeichert – leer lassen zum Beibehalten' : 'Optional';
          document.getElementById('victron_clear_password').checked = false;
          for(let i=0;i<2;++i)document.getElementById('victron_relay_'+i).value = (data.victron_relay_names || [])[i] || ('GX Relais '+(i+1));
          document.getElementById('home_water_source').value = data.home_water_source || 0;
          populateFavoriteSelect('home_favorite_0', data.relays, data.dimmers, data.home_favorite ? data.home_favorite[0] : 0, data.victron_relays);
          populateFavoriteSelect('home_favorite_1', data.relays, data.dimmers, data.home_favorite ? data.home_favorite[1] : 0, data.victron_relays);

          // Tabs
          tabOrder = (data.tab_order && data.tab_order.length) ? data.tab_order.slice() : tabMeta.map(t => t.id);
          tabMeta.forEach(m => {
            if (!tabOrder.includes(m.id)) tabOrder.push(m.id);
          });
          tabEnabled = {};
          if (data.tab_enabled) {
            for (let i = 0; i < data.tab_enabled.length; i++) {
              tabEnabled[i] = !!data.tab_enabled[i];
            }
          }
          renderTabReorderList();
        })
        .catch(err => {
          console.error(err);
          showToast("Fehler beim Laden der Einstellungen", true);
        });
    }

    function populateRelaySelect(elemId, relays, selectedVal) {
      const sel = document.getElementById(elemId);
      if (!sel) return;
      sel.innerHTML = '<option value="-1">Nicht verfügbar</option>';
      if (relays) {
        relays.forEach(r => {
          const opt = document.createElement('option');
          opt.value = r.id;
          opt.text = (r.is_wrelay ? '📶 ' : '🔘 ') + r.name;
          if (r.id === selectedVal) opt.selected = true;
          sel.appendChild(opt);
        });
      }
    }

    function populateDimmerSelect(elemId, dimmers, selectedVal) {
      const sel = document.getElementById(elemId);
      if (!sel) return;
      sel.innerHTML = '';
      if (dimmers) {
        dimmers.forEach(d => {
          const opt = document.createElement('option');
          opt.value = d.id;
          opt.text = '💡 ' + d.name;
          if (d.id === selectedVal) opt.selected = true;
          sel.appendChild(opt);
        });
      }
    }

    function populateTempSensorsSelect(elemId, sensors, selectedVal, includeNone) {
      const sel = document.getElementById(elemId);
      if (!sel) return;
      sel.innerHTML = includeNone ? '<option value="-1">Nicht anzeigen</option>' : '';
      if (sensors) {
        sensors.forEach(s => {
          const opt = document.createElement('option');
          opt.value = s.idx;
          opt.text = s.name + (s.val !== undefined ? ` (${s.val.toFixed(1)} °C)` : '');
          if (s.idx === selectedVal) opt.selected = true;
          sel.appendChild(opt);
        });
      }
    }

    function populateHeaterTypes(heaterTypes, selectedType) {
      const sel = document.getElementById('heater_type');
      if (!sel) return;
      sel.innerHTML = '';
      const list = heaterTypes || [
        { id: 0, name: 'Autoterm (Air 2D/4D)' },
        { id: 1, name: 'Truma (Gas/Diesel)' },
        { id: 2, name: 'Truma + Elektro (Combi E)' },
        { id: 4, name: 'Webasto (W-Bus)' },
        { id: 3, name: 'Keine / Deaktiviert' }
      ];
      list.forEach(h => {
        const opt = document.createElement('option');
        opt.value = h.id;
        opt.text = h.name;
        if (h.id === selectedType) opt.selected = true;
        sel.appendChild(opt);
      });
      sel.onchange = function() {
        const val = parseInt(this.value);
        if (val === 0 || val === 4) { // Autoterm or Webasto
          tabEnabled[4] = true;
          tabEnabled[10] = false;
        } else if (val === 1 || val === 2) { // Truma (pure or electric)
          tabEnabled[4] = false;
          tabEnabled[10] = true;
        } else { // None
          tabEnabled[4] = false;
          tabEnabled[10] = false;
        }
        renderTabReorderList();
      };
    }

    function populateFavoriteSelect(elemId, relays, dimmers, selectedVal, gxRelays) {
      const sel = document.getElementById(elemId);
      if (!sel) return;
      sel.innerHTML = '<option value="0">Aus (Nicht belegt)</option>';
      if (relays) {
        relays.forEach(r => {
          if (!r.is_wrelay) {
            const opt = document.createElement('option');
            opt.value = r.id + 1; // 1..8
            opt.text = '🔘 ' + r.name;
            if (opt.value == selectedVal) opt.selected = true;
            sel.appendChild(opt);
          }
        });
      }
      if (dimmers) {
        dimmers.forEach(d => {
          const opt = document.createElement('option');
          opt.value = d.id + 9; // 9..16
          opt.text = '💡 ' + d.name;
          if (opt.value == selectedVal) opt.selected = true;
          sel.appendChild(opt);
        });
      }
      (gxRelays || []).forEach(r => {
        const opt = document.createElement('option'); opt.value = r.id; opt.text = 'GX: ' + r.name;
        if (Number(r.id) === Number(selectedVal)) opt.selected = true;
        sel.appendChild(opt);
      });
    }

    function updateTankInputs() {
      const cnt = parseInt(document.getElementById('tank_count').value) || 1;
      for (let i = 0; i < 4; i++) {
        const w = document.getElementById('wrap_tank' + i);
        if (w) w.style.display = (i < cnt) ? 'block' : 'none';
      }
    }

    function buildRelaysList(names, visible, showExt) {
      const wrap = document.getElementById('relaysList');
      wrap.innerHTML = '';
      const count = showExt ? 16 : 8;
      for (let i = 0; i < count; i++) {
        const name = (names && names[i]) ? names[i] : `Relais ${i + 1}`;
        const vis = visible ? !!visible[i] : true;
        const row = document.createElement('div');
        row.className = 'toggle-row';
        row.innerHTML = `
          <div class="toggle-info"><div class="toggle-title">🔘 ${name}</div></div>
          <label class="switch">
            <input type="checkbox" id="switch_vis_${i}" ${vis ? 'checked' : ''}>
            <span class="slider"></span>
          </label>
        `;
        wrap.appendChild(row);
      }
    }

    function toggleRelayExt(checked) {
      buildRelaysList(currentSettings.switch_names, currentSettings.switch_visible, checked);
    }

    function buildDimmersList(names, visible, isDimmyPro) {
      const wrap = document.getElementById('dimmersList');
      wrap.innerHTML = '';
      const count = isDimmyPro ? 10 : 8;
      for (let i = 0; i < count; i++) {
        let def = (i < 8) ? `Dimmer ${i + 1}` : `RGBW ${i - 7}`;
        if (isDimmyPro && i < 8) def = `Dimmer ${i + 8}`;
        const name = (names && names[i]) ? names[i] : def;
        const vis = visible ? !!visible[i] : true;
        const icon = (i >= 8) ? '🎨' : '💡';
        const row = document.createElement('div');
        row.className = 'toggle-row';
        row.innerHTML = `
          <div class="toggle-info"><div class="toggle-title">${icon} ${name}</div></div>
          <label class="switch">
            <input type="checkbox" id="dimmer_vis_${i}" ${vis ? 'checked' : ''}>
            <span class="slider"></span>
          </label>
        `;
        wrap.appendChild(row);
      }
    }

    function toggleDimmyPro(checked) {
      buildDimmersList(currentSettings.dimmer_names, currentSettings.dimmer_visible, checked);
    }

    function buildWRelaysList(names, visible) {
      const wrap = document.getElementById('wrelaysList');
      wrap.innerHTML = '';
      for (let i = 0; i < 8; i++) {
        const name = (names && names[i]) ? names[i] : `WLAN Relais ${i + 1}`;
        const vis = visible ? !!visible[i] : true;
        const row = document.createElement('div');
        row.className = 'toggle-row';
        row.innerHTML = `
          <div class="toggle-info"><div class="toggle-title">📶 ${name}</div></div>
          <label class="switch">
            <input type="checkbox" id="wrelay_vis_${i}" ${vis ? 'checked' : ''}>
            <span class="slider"></span>
          </label>
        `;
        wrap.appendChild(row);
      }
    }

    function renderTabReorderList() {
      const wrap = document.getElementById('tabReorderList');
      wrap.innerHTML = '';
      tabOrder.forEach((tabId, idx) => {
        if (tabId >= tabMeta.length) return;
        const meta = tabMeta[tabId];
        const isEn = tabEnabled[tabId] !== undefined ? tabEnabled[tabId] : true;
        const item = document.createElement('div');
        item.className = 'reorder-item';
        item.innerHTML = `
          <div class="reorder-left">
            <span class="reorder-icon">${meta.icon}</span>
            <span class="reorder-name">${meta.name}</span>
          </div>
          <div style="display:flex; align-items:center; gap:10px;">
            <label class="switch">
              <input type="checkbox" onchange="tabEnabled[${tabId}] = this.checked" ${isEn ? 'checked' : ''}>
              <span class="slider"></span>
            </label>
            <div class="reorder-btns">
              <button type="button" class="btn-arrow" onclick="moveTab(${idx}, -1)" ${idx === 0 ? 'disabled' : ''}>▲</button>
              <button type="button" class="btn-arrow" onclick="moveTab(${idx}, 1)" ${idx === tabOrder.length - 1 ? 'disabled' : ''}>▼</button>
            </div>
          </div>
        `;
        wrap.appendChild(item);
      });
    }

    function moveTab(idx, dir) {
      const target = idx + dir;
      if (target < 0 || target >= tabOrder.length) return;
      const tmp = tabOrder[idx];
      tabOrder[idx] = tabOrder[target];
      tabOrder[target] = tmp;
      renderTabReorderList();
    }

    function saveSettings() {
      const themeTab = document.getElementById('tab-theme');
      if (themeTab && themeTab.classList.contains('active')) {
        saveAndApplyTheme();
        return;
      }
      const btn = document.getElementById('btnSave');
      btn.innerText = "⏳ Speichere...";
      btn.disabled = true;

      const payload = {
        wifi_ssid: document.getElementById('wifi_ssid').value,
        wifi_pass: document.getElementById('wifi_pass').value,
        vanpi_ip: document.getElementById('vanpi_ip').value,

        dark_mode: document.getElementById('dark_mode').checked,
        debug_mode: document.getElementById('debug_mode').checked,
        display_brightness: parseInt(document.getElementById('display_brightness').value),
        display_timeout: parseInt(document.getElementById('display_timeout').value),
        screensaver_enabled: document.getElementById('screensaver_enabled').checked,
        screensaver_brightness: parseInt(document.getElementById('screensaver_brightness').value),

        tank_count: parseInt(document.getElementById('tank_count').value),
        tank_show_liters: document.getElementById('tank_show_liters').checked,
        tank_max: [
          parseInt(document.getElementById('tank_max_0').value) || 100,
          parseInt(document.getElementById('tank_max_1').value) || 100,
          parseInt(document.getElementById('tank_max_2').value) || 100,
          parseInt(document.getElementById('tank_max_3').value) || 100
        ],
        tank_unit: [
          parseInt(document.getElementById('tank_unit_0').value) || 0,
          parseInt(document.getElementById('tank_unit_1').value) || 0,
          parseInt(document.getElementById('tank_unit_2').value) || 0,
          parseInt(document.getElementById('tank_unit_3').value) || 0
        ],

        pump_relay: parseInt(document.getElementById('pump_relay').value),
        drain_relay: parseInt(document.getElementById('drain_relay').value),
        heater_type: parseInt(document.getElementById('heater_type').value) || 0,
        heater_temp_buttons: document.getElementById('heater_temp_buttons').checked,

        show_wrelay: document.getElementById('show_wrelay').checked,
        show_relay_ext: document.getElementById('show_relay_ext').checked,
        dimmy_pro: document.getElementById('dimmy_pro').checked,
        switch_visible: [],
        dimmer_visible: [],
        wrelay_visible: [],

        outdoor_temp_sensor: parseInt(document.getElementById('outdoor_temp_sensor').value),
        indoor_temp_sensor: parseInt(document.getElementById('indoor_temp_sensor').value),
        fridge_temp_sensor: parseInt(document.getElementById('fridge_temp_sensor').value),
        temps_custom_sensor: [
          parseInt(document.getElementById('temps_custom_sensor_0').value),
          parseInt(document.getElementById('temps_custom_sensor_1').value)
        ],

        warn_frost_temp: parseFloat(document.getElementById('warn_frost_temp').value),
        warn_bat_soc: parseInt(document.getElementById('warn_bat_soc').value),
        warn_bat_yellow_soc: parseInt(document.getElementById('warn_bat_yellow_soc').value),
        warn_fresh_min: parseInt(document.getElementById('warn_fresh_min').value),
        warn_waste_max: parseInt(document.getElementById('warn_waste_max').value),
        power_show_solar: document.getElementById('power_show_solar').checked,
        power_show_shore: document.getElementById('power_show_shore').checked,
        power_show_alt: document.getElementById('power_show_alt').checked,
        power_calc_load: document.getElementById('power_calc_load').checked,
        solar_max_w: parseFloat(document.getElementById('solar_max_w').value) || 200,
        shore_max_w: parseFloat(document.getElementById('shore_max_w').value) || 2000,
        alt_max_w: parseFloat(document.getElementById('alt_max_w').value) || 600,
        bat_capacity_ah: parseFloat(document.getElementById('bat_capacity_ah').value) || 100,

        home_show_battery: document.getElementById('home_show_battery').checked,
        home_show_starter: document.getElementById('home_show_starter').checked,
        home_show_solar: document.getElementById('home_show_solar').checked,
        home_show_water: document.getElementById('home_show_water').checked,
        home_show_victron: document.getElementById('home_show_victron').checked,
        victron_enabled: document.getElementById('victron_enabled').checked,
        victron_keepalive: document.getElementById('victron_keepalive').checked,
        victron_starter: document.getElementById('victron_starter').value === '1',
        victron_battery_instance: document.getElementById('victron_battery_instance').value === '' ? -1 : Number(document.getElementById('victron_battery_instance').value),
        victron_host: document.getElementById('victron_host').value.trim(),
        victron_port: Number(document.getElementById('victron_port').value),
        victron_portal: document.getElementById('victron_portal').value.trim(),
        victron_instance: document.getElementById('victron_instance').value === '' ? -1 : Number(document.getElementById('victron_instance').value),
        victron_user: document.getElementById('victron_user').value,
        victron_password: document.getElementById('victron_password').value,
        victron_clear_password: document.getElementById('victron_clear_password').checked,
        victron_relay_names: [document.getElementById('victron_relay_0').value, document.getElementById('victron_relay_1').value],
        home_water_source: parseInt(document.getElementById('home_water_source').value),
        home_favorite: [
          parseInt(document.getElementById('home_favorite_0').value),
          parseInt(document.getElementById('home_favorite_1').value)
        ],

        tab_order: tabOrder,
        tab_enabled: []
      };

      for (let i = 0; i < 16; i++) {
        const sw = document.getElementById('switch_vis_' + i);
        if (sw) payload.switch_visible.push(sw.checked);
        else if (currentSettings.switch_visible && i < currentSettings.switch_visible.length) payload.switch_visible.push(currentSettings.switch_visible[i]);
        else payload.switch_visible.push(true);
      }
      for (let i = 0; i < 10; i++) {
        const dm = document.getElementById('dimmer_vis_' + i);
        if (dm) payload.dimmer_visible.push(dm.checked);
        else if (currentSettings.dimmer_visible && i < currentSettings.dimmer_visible.length) payload.dimmer_visible.push(currentSettings.dimmer_visible[i]);
        else payload.dimmer_visible.push(true);
      }
      for (let i = 0; i < 8; i++) {
        const wr = document.getElementById('wrelay_vis_' + i);
        payload.wrelay_visible.push(wr ? wr.checked : true);
      }

      for (let i = 0; i < tabMeta.length; i++) {
        payload.tab_enabled.push(tabEnabled[i] !== undefined ? tabEnabled[i] : i !== 11);
      }

      fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      })
      .then(async r => {
        const result = await r.json();
        if (!r.ok || result.error) throw new Error(result.error || 'Speichern fehlgeschlagen');
        return result;
      })
      .then(res => {
        document.getElementById('victron_password').value = '';
        document.getElementById('victron_clear_password').checked = false;
        btn.innerText = "💾 Gespeichert!";
        showToast("✅ Einstellungen erfolgreich auf Display & NVS gespeichert!");
        setTimeout(() => {
          btn.innerText = "💾 Einstellungen speichern";
          btn.disabled = false;
        }, 1500);
      })
      .catch(err => {
        console.error(err);
        btn.innerText = "❌ Fehler";
        btn.disabled = false;
        showToast("Fehler beim Speichern: " + err.message, true);
      });
    }

    function rebootEsp() {
      if (!confirm("ESP32-S3 jetzt neu starten?")) return;
      fetch('/api/restart', { method: 'POST' })
        .then(() => {
          showToast("ESP32 wird neu gestartet...");
          setTimeout(() => { location.reload(); }, 4000);
        });
    }

    function scanWifi() {
      const modal = document.getElementById('wifiModal');
      const list = document.getElementById('wifiList');
      modal.classList.add('show');
      list.innerHTML = '<div style="padding:12px; text-align:center; color:#94a3b8;">🔍 Suche WLAN-Netzwerke...</div>';

      fetch('/api/wifi/scan')
        .then(r => r.json())
        .then(networks => {
          if (!networks || !networks.length) {
            list.innerHTML = '<div style="padding:12px; text-align:center; color:#94a3b8;">Keine Netzwerke gefunden</div>';
            return;
          }
          list.innerHTML = '';
          networks.forEach(net => {
            const item = document.createElement('div');
            item.className = 'wifi-item';
            item.innerHTML = `
              <span class="wifi-name">${net.ssid}</span>
              <span class="wifi-rssi">${net.rssi} dBm ${net.secure ? '🔒' : ''}</span>
            `;
            item.onclick = () => {
              document.getElementById('wifi_ssid').value = net.ssid;
              closeWifiModal();
              document.getElementById('wifi_pass').focus();
            };
            list.appendChild(item);
          });
        })
        .catch(err => {
          list.innerHTML = '<div style="padding:12px; text-align:center; color:#ef4444;">Scan fehlgeschlagen</div>';
        });
    }

    function closeWifiModal() {
      document.getElementById('wifiModal').classList.remove('show');
    }

    // Init
    function refreshGxFavorites() {
      for(const id of ['home_favorite_0','home_favorite_1']) {
        const select=document.getElementById(id);
        for(let i=0;i<2;++i) {
          const name=document.getElementById('victron_relay_'+i).value.trim() || ('GX Relais '+(i+1));
          for(const option of select.options)if(Number(option.value)===17+i)option.textContent='GX: '+name;
        }
      }
    }
    function homeEnergyChoice(multi) {
      if (multi && document.getElementById('home_show_victron').checked) {
        document.getElementById('home_show_water').checked = false; tabEnabled[11] = true; renderTabReorderList();
      } else if (!multi && document.getElementById('home_show_water').checked) {
        document.getElementById('home_show_victron').checked = false;
      }
    }
    function pollVictronStatus() {
      if (!document.getElementById('tab-victron').classList.contains('active')) return;
      fetch('/api/victron').then(r => { if (!r.ok) throw new Error(); return r.json(); }).then(s => {
        document.getElementById('victron_status').textContent = s.status + '\nPortal: ' + (s.portal || 'Automatisch / warte') +
          ' | MultiPlus: ' + (s.instance >= 0 ? s.instance : 'Automatisch / warte') +
          (document.getElementById('victron_starter').value === '1' ? '\nStarter GX: ' + (s.starter_ambiguous ? 'Mehrere Monitore – Geräte-ID wählen' : s.starter_voltage != null ? Number(s.starter_voltage).toFixed(1) + ' V' : 'Keine aktuellen Daten') : '');
      }).catch(() => { document.getElementById('victron_status').textContent = 'Display nicht erreichbar'; });
    }
    setInterval(pollVictronStatus, 3000);
        // ================= THEME SYSTEM =================
    const themePresets = {
      slate: {
        name: "Camper Slate",
        color_primary: "#0EA5E9",
        color_dimmer:  "#F59E0B",
        color_relay:   "#10B981",
        color_tank:    "#06B6D4",
        color_bg:      "#101318",
        color_card:    "#181C24",
        color_border:  "#272D3B",
        color_text:    "#F1F5F9",
        color_muted:   "#94A3B8",
        color_track:   "#12151C",
        color_btn_text: "#F1F5F9",
        color_btn_text_active: "#FFFFFF",
        radius_card: 14,
        radius_button: 10,
        border_width: 1
      },
      amber: {
        name: "Nordic Amber",
        color_primary: "#F59E0B",
        color_dimmer:  "#FBBF24",
        color_relay:   "#10B981",
        color_tank:    "#38BDF8",
        color_bg:      "#12100E",
        color_card:    "#1C1815",
        color_border:  "#332A22",
        color_text:    "#FEF3C7",
        color_muted:   "#A89F91",
        color_track:   "#14110F",
        color_btn_text: "#FEF3C7",
        color_btn_text_active: "#FFFFFF",
        radius_card: 14,
        radius_button: 10,
        border_width: 1
      },
      forest: {
        name: "Forest Green",
        color_primary: "#10B981",
        color_dimmer:  "#EAB308",
        color_relay:   "#22C55E",
        color_tank:    "#06B6D4",
        color_bg:      "#0C1410",
        color_card:    "#14221B",
        color_border:  "#1F382B",
        color_text:    "#ECFDF5",
        color_muted:   "#86A796",
        color_track:   "#0F1A14",
        color_btn_text: "#ECFDF5",
        color_btn_text_active: "#FFFFFF",
        radius_card: 16,
        radius_button: 12,
        border_width: 1
      },
      ocean: {
        name: "Ocean Breeze",
        color_primary: "#06B6D4",
        color_dimmer:  "#F59E0B",
        color_relay:   "#10B981",
        color_tank:    "#38BDF8",
        color_bg:      "#08131D",
        color_card:    "#0F2231",
        color_border:  "#1B3B55",
        color_text:    "#F0FDFA",
        color_muted:   "#7EA4BE",
        color_track:   "#0B1924",
        color_btn_text: "#F0FDFA",
        color_btn_text_active: "#FFFFFF",
        radius_card: 14,
        radius_button: 10,
        border_width: 1
      },
      minimal: {
        name: "Modern Minimal",
        color_primary: "#FFFFFF",
        color_dimmer:  "#E2E8F0",
        color_relay:   "#10B981",
        color_tank:    "#94A3B8",
        color_bg:      "#000000",
        color_card:    "#111111",
        color_border:  "#2B2B2B",
        color_text:    "#FFFFFF",
        color_muted:   "#888888",
        color_track:   "#181818",
        color_btn_text: "#FFFFFF",
        color_btn_text_active: "#000000",
        radius_card: 0,
        radius_button: 0,
        border_width: 1
      },
      sunset: {
        name: "Sunset Terracotta",
        color_primary: "#F97316",
        color_dimmer:  "#FB923C",
        color_relay:   "#10B981",
        color_tank:    "#38BDF8",
        color_bg:      "#141110",
        color_card:    "#221C1A",
        color_border:  "#3A2F2B",
        color_text:    "#FFF7ED",
        color_muted:   "#A89B94",
        color_track:   "#171311",
        color_btn_text: "#FFF7ED",
        color_btn_text_active: "#FFFFFF",
        radius_card: 14,
        radius_button: 10,
        border_width: 1
      }
    };

    function setThemeFormValues(d) {
      if (!d) return;
      const setVal = (colId, hexId, val) => {
        if (!val) return;
        const h = val.startsWith('#') ? val.toUpperCase() : ('#' + val).toUpperCase();
        const cEl = document.getElementById(colId);
        const hEl = document.getElementById(hexId);
        if (cEl) cEl.value = h.substring(0, 7);
        if (hEl) hEl.value = h.substring(0, 7);
      };
      setVal('theme_col_primary', 'theme_hex_primary', d.color_primary);
      setVal('theme_col_dimmer',  'theme_hex_dimmer',  d.color_dimmer);
      setVal('theme_col_relay',   'theme_hex_relay',   d.color_relay);
      setVal('theme_col_tank',    'theme_hex_tank',    d.color_tank);
      setVal('theme_col_bg',      'theme_hex_bg',      d.color_bg);
      setVal('theme_col_card',    'theme_hex_card',    d.color_card);
      setVal('theme_col_border',  'theme_hex_border',  d.color_border);
      setVal('theme_col_text',    'theme_hex_text',    d.color_text);
      setVal('theme_col_muted',   'theme_hex_muted',   d.color_muted);
      setVal('theme_col_track',   'theme_hex_track',   d.color_track);
      setVal('theme_col_btn_text', 'theme_hex_btn_text', d.color_btn_text);
      setVal('theme_col_btn_text_active', 'theme_hex_btn_text_active', d.color_btn_text_active);

      if (d.radius_card !== undefined) {
        document.getElementById('theme_radius_card').value = d.radius_card;
        document.getElementById('val_radius_card').innerText = d.radius_card + ' px';
      }
      if (d.radius_button !== undefined) {
        document.getElementById('theme_radius_button').value = d.radius_button;
        document.getElementById('val_radius_button').innerText = d.radius_button + ' px';
      }
      if (d.border_width !== undefined) {
        document.getElementById('theme_border_width').value = d.border_width;
        document.getElementById('val_border_width').innerText = d.border_width + ' px';
      }
    }

    function syncColor(colId, hexId) {
      const col = document.getElementById(colId).value.toUpperCase();
      document.getElementById(hexId).value = col;
      document.getElementById('theme_preset').value = 'custom';
      updateThemePreview();
    }

    function syncHex(hexId, colId) {
      let hex = document.getElementById(hexId).value.trim();
      if (!hex.startsWith('#')) hex = '#' + hex;
      if (/^#[0-9A-F]{6}$/i.test(hex)) {
        document.getElementById(colId).value = hex;
        document.getElementById('theme_preset').value = 'custom';
        updateThemePreview();
      }
    }

    function applyThemePreset(key) {
      if (key === 'custom') return;
      const p = themePresets[key];
      if (!p) return;
      setThemeFormValues(p);
      updateThemePreview();
    }

    function resetThemeToDefault() {
      if (!confirm("Theme wirklich auf Werkseinstellungen (Camper Slate) zurücksetzen?")) return;
      applyThemePreset('slate');
      document.getElementById('theme_preset').value = 'slate';
      showToast("Auf Werkseinstellungen zurückgesetzt! Klicke unten auf 'Theme speichern & anwenden'.");
    }

    function updateThemePreview() {
      const colPrimary = document.getElementById('theme_hex_primary').value || '#0EA5E9';
      const colDimmer  = document.getElementById('theme_hex_dimmer').value  || '#F59E0B';
      const colRelay   = document.getElementById('theme_hex_relay').value   || '#10B981';
      const colTank    = document.getElementById('theme_hex_tank').value    || '#06B6D4';
      const colBg      = document.getElementById('theme_hex_bg').value      || '#101318';
      const colCard    = document.getElementById('theme_hex_card').value    || '#181C24';
      const colBorder  = document.getElementById('theme_hex_border').value  || '#272D3B';
      const colText    = document.getElementById('theme_hex_text').value    || '#F1F5F9';
      const colMuted   = document.getElementById('theme_hex_muted').value   || '#94A3B8';
      const colTrack   = document.getElementById('theme_hex_track').value   || '#12151C';
      const colBtnText = document.getElementById('theme_hex_btn_text').value || '#F1F5F9';
      const colBtnTextActive = document.getElementById('theme_hex_btn_text_active').value || '#FFFFFF';
      const rCard      = parseInt(document.getElementById('theme_radius_card').value) || 0;
      const rBtn       = parseInt(document.getElementById('theme_radius_button').value) || 0;
      const bWidth     = parseInt(document.getElementById('theme_border_width').value) || 0;

      const app         = document.getElementById('themePreviewApp');
      const header      = document.getElementById('themePreviewHeader');
      const headerText  = document.getElementById('themePreviewHeaderText');
      const headerSub   = document.getElementById('themePreviewHeaderSub');
      const headerBadge = document.getElementById('themePreviewHeaderBadge');
      const card        = document.getElementById('themePreviewCard');
      const dimLabel    = document.getElementById('themePreviewDimmerLabel');
      const dimPct      = document.getElementById('themePreviewDimmerPct');
      const track       = document.getElementById('themePreviewTrack');
      const dimBar      = document.getElementById('themePreviewDimmerBar');
      const relayCard   = document.getElementById('themePreviewRelayCard');
      const tankCard    = document.getElementById('themePreviewTankCard');
      const tankLabel   = document.getElementById('themePreviewTankLabel');
      const tankPct     = document.getElementById('themePreviewTankPct');
      const tankTrack   = document.getElementById('themePreviewTankTrack');
      const tankBar     = document.getElementById('themePreviewTankBar');
      const btn         = document.getElementById('themePreviewBtn');

      if (app) app.style.backgroundColor = colBg;
      if (header) {
        header.style.backgroundColor = colCard;
        header.style.borderBottomColor = colBorder;
        header.style.borderBottomWidth = bWidth + 'px';
      }
      if (headerText) headerText.style.color = colText;
      if (headerSub) headerSub.style.color = colMuted;
      if (headerBadge) headerBadge.style.backgroundColor = colPrimary;

      if (card) {
        card.style.backgroundColor = colCard;
        card.style.borderColor = colBorder;
        card.style.borderWidth = bWidth + 'px';
        card.style.borderRadius = rCard + 'px';
      }
      if (dimLabel) dimLabel.style.color = colText;
      if (dimPct) dimPct.style.color = colDimmer;
      if (track) {
        track.style.backgroundColor = colTrack;
        track.style.borderColor = colBorder;
        track.style.borderWidth = bWidth + 'px';
        track.style.borderRadius = rBtn + 'px';
      }
      if (dimBar) {
        dimBar.style.backgroundColor = colDimmer;
        dimBar.style.borderRadius = rBtn + 'px';
      }

      if (relayCard) {
        relayCard.style.backgroundColor = colRelay;
        relayCard.style.borderColor = colRelay;
        relayCard.style.borderRadius = rBtn + 'px';
        relayCard.style.color = colBtnTextActive;
      }

      if (tankCard) {
        tankCard.style.backgroundColor = colCard;
        tankCard.style.borderColor = colBorder;
        tankCard.style.borderWidth = bWidth + 'px';
        tankCard.style.borderRadius = rCard + 'px';
      }
      if (tankLabel) tankLabel.style.color = colMuted;
      if (tankPct) tankPct.style.color = colText;
      if (tankTrack) tankTrack.style.backgroundColor = colTrack;
      if (tankBar) tankBar.style.backgroundColor = colTank;

      if (btn) {
        btn.style.backgroundColor = colPrimary;
        btn.style.color = colBtnTextActive;
        btn.style.borderRadius = rBtn + 'px';
      }
    }

    function loadTheme() {
      fetch('/api/theme')
        .then(r => r.json())
        .then(d => {
          setThemeFormValues(d);
          let matched = 'custom';
          for (const k in themePresets) {
            const p = themePresets[k];
            if (p.color_primary.toUpperCase() === (d.color_primary || '').toUpperCase() &&
                p.color_dimmer.toUpperCase()  === (d.color_dimmer  || '').toUpperCase() &&
                p.color_relay.toUpperCase()   === (d.color_relay   || '').toUpperCase() &&
                p.color_tank.toUpperCase()    === (d.color_tank    || '').toUpperCase() &&
                p.color_bg.toUpperCase()      === (d.color_bg      || '').toUpperCase() &&
                p.radius_card === d.radius_card) {
              matched = k;
              break;
            }
          }
          document.getElementById('theme_preset').value = matched;
          updateThemePreview();
        })
        .catch(() => updateThemePreview());
    }

    function exportThemeFile() {
      const themeObj = getThemePayload(false);
      delete themeObj.restart;
      const themeName = themeObj.name;
      const slug = themeName.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/(^-|-$)/g, '');
      const blob = new Blob([JSON.stringify(themeObj, null, 2)], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = "camperui-theme-" + slug + ".json";
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      showToast("Theme-Datei heruntergeladen!");
    }

    function importThemeFile(input) {
      if (!input.files || !input.files[0]) return;
      const file = input.files[0];
      const reader = new FileReader();
      reader.onload = function(e) {
        try {
          const d = JSON.parse(e.target.result);
          if (d.color_primary) {
            setThemeFormValues(d);
            document.getElementById('theme_preset').value = 'custom';
            updateThemePreview();
            showToast("Theme erfolgreich geladen!");
          } else {
            showToast("Ung&uuml;ltiges Theme-Format", true);
          }
        } catch (err) {
          showToast("Fehler beim Parsen der JSON-Datei", true);
        }
        input.value = '';
      };
      reader.readAsText(file);
    }

    function getThemePayload(restart = true) {
      const pEl = document.getElementById('theme_preset');
      const themeName = (pEl && pEl.selectedIndex >= 0 && pEl.value !== 'custom' && pEl.options[pEl.selectedIndex]) ? pEl.options[pEl.selectedIndex].text : "Custom Theme";
      const val = (id, def) => {
        const el = document.getElementById(id);
        return (el && el.value) ? el.value : def;
      };
      const intVal = (id, def) => {
        const el = document.getElementById(id);
        return el ? (parseInt(el.value) || def) : def;
      };
      return {
        name: themeName,
        color_primary: val('theme_hex_primary', '#0EA5E9'),
        color_dimmer:  val('theme_hex_dimmer',  '#F59E0B'),
        color_relay:   val('theme_hex_relay',   '#10B981'),
        color_tank:    val('theme_hex_tank',    '#06B6D4'),
        color_bg:      val('theme_hex_bg',      '#101318'),
        color_card:    val('theme_hex_card',    '#181C24'),
        color_border:  val('theme_hex_border',  '#272D3B'),
        color_text:    val('theme_hex_text',    '#F1F5F9'),
        color_muted:   val('theme_hex_muted',   '#94A3B8'),
        color_track:   val('theme_hex_track',   '#12151C'),
        color_btn_text: val('theme_hex_btn_text', '#F1F5F9'),
        color_btn_text_active: val('theme_hex_btn_text_active', '#FFFFFF'),
        radius_card:   intVal('theme_radius_card', 14),
        radius_button: intVal('theme_radius_button', 10),
        border_width:  intVal('theme_border_width', 1),
        restart: restart
      };
    }

    function saveAndApplyTheme() {
      const btn = document.getElementById('btnSave');
      if (btn) {
        btn.innerHTML = '<span>⏳</span> Speichere Theme...';
        btn.disabled = true;
      }

      const payload = getThemePayload(false);

      fetch('/api/theme', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      })
      .then(async r => {
        const res = await r.json();
        if (!r.ok || res.error) throw new Error(res.error || 'Fehler beim Speichern');
        return res;
      })
      .then(d => {
        if (btn) {
          btn.innerHTML = '<span>💾</span> Theme gespeichert!';
        }
        showToast("✅ Theme erfolgreich gespeichert und auf Display angewendet!");
        setTimeout(() => {
          if (btn) {
            btn.innerHTML = '<span>💾</span> Theme speichern &amp; anwenden';
            btn.disabled = false;
          }
        }, 1500);
      })
      .catch(err => {
        showToast("Fehler beim Speichern: " + err.message, true);
        if (btn) {
          btn.innerHTML = '<span>❌</span> Fehler';
          setTimeout(() => {
            btn.innerHTML = '<span>💾</span> Theme speichern &amp; anwenden';
            btn.disabled = false;
          }, 2000);
        }
      });
    }

    window.addEventListener('DOMContentLoaded', () => { loadSettings(); loadTheme(); });
  </script>
</body>
</html>
)rawliteral";

#endif // WEB_SETTINGS_HTML_H
