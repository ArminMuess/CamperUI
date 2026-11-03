#ifndef WEB_TERMINAL_HTML_H
#define WEB_TERMINAL_HTML_H

#include <Arduino.h>

static const char TERMINAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CamperUI - Web-Terminal & Status</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: #0f1218;
      color: #f1f5f9;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      padding: 16px;
    }
    .container {
      max-width: 1200px;
      width: 100%;
      margin: 0 auto;
      display: flex;
      flex-direction: column;
      gap: 14px;
      flex: 1;
    }
    header {
      background-color: #181d26;
      border: 1px solid #283141;
      border-radius: 12px;
      padding: 14px 20px;
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
    }
    .header-left {
      display: flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
    }
    h1 {
      font-size: 19px;
      font-weight: 700;
      color: #f8fafc;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .badge {
      font-size: 12px;
      font-weight: 600;
      padding: 3px 10px;
      border-radius: 14px;
      background: rgba(14, 165, 233, 0.15);
      color: #38bdf8;
      border: 1px solid rgba(14, 165, 233, 0.3);
    }
    .badge.green {
      background: rgba(16, 185, 129, 0.15);
      color: #34d399;
      border-color: rgba(16, 185, 129, 0.3);
    }
    .nav-btn {
      background: #222937;
      color: #cbd5e1;
      text-decoration: none;
      font-size: 13px;
      font-weight: 600;
      padding: 8px 14px;
      border-radius: 8px;
      border: 1px solid #334155;
      transition: all 0.2s;
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    .nav-btn:hover {
      background: #334155;
      color: #fff;
    }

    /* System Stats Grid */
    .stats-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 10px;
    }
    .stat-card {
      background: #181d26;
      border: 1px solid #283141;
      border-radius: 10px;
      padding: 10px 14px;
    }
    .stat-label {
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      color: #94a3b8;
      margin-bottom: 2px;
    }
    .stat-val {
      font-size: 14px;
      font-weight: 600;
      color: #f8fafc;
    }
    .stat-sub {
      font-size: 11px;
      color: #64748b;
      margin-top: 2px;
    }

    /* Terminal Controls Toolbar */
    .toolbar {
      background: #181d26;
      border: 1px solid #283141;
      border-radius: 10px;
      padding: 10px 14px;
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
    }
    .btn-group {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
    }
    .btn {
      background: #222937;
      color: #f1f5f9;
      border: 1px solid #3b465b;
      border-radius: 6px;
      padding: 7px 12px;
      font-size: 13px;
      font-weight: 500;
      cursor: pointer;
      transition: all 0.15s;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      text-decoration: none;
    }
    .btn:hover {
      background: #2e3749;
      border-color: #4b5872;
    }
    .btn.primary {
      background: #0284c7;
      border-color: #0369a1;
      color: #fff;
    }
    .btn.primary:hover {
      background: #0369a1;
    }
    .btn.success {
      background: #059669 !important;
      border-color: #047857 !important;
      color: #fff !important;
    }
    .btn.danger {
      background: #b91c1c;
      border-color: #991b1b;
      color: #fff;
    }
    .btn.danger:hover {
      background: #991b1b;
    }
    .toggle-label {
      font-size: 13px;
      color: #cbd5e1;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      cursor: pointer;
      user-select: none;
      margin-left: 6px;
    }

    /* Quick command buttons bar */
    .cmd-bar {
      background: #141820;
      border: 1px solid #283141;
      border-radius: 8px;
      padding: 8px 12px;
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: 8px;
    }
    .cmd-title {
      font-size: 12px;
      font-weight: 600;
      color: #94a3b8;
      margin-right: 2px;
    }
    .btn-cmd {
      background: #1e2533;
      color: #38bdf8;
      border: 1px solid #2f3b4e;
      border-radius: 6px;
      padding: 5px 9px;
      font-size: 12px;
      font-family: monospace;
      cursor: pointer;
      transition: all 0.15s;
    }
    .btn-cmd:hover {
      background: #2b3548;
      border-color: #38bdf8;
      color: #fff;
    }
    .cmd-input-wrap {
      display: inline-flex;
      align-items: center;
      gap: 4px;
      margin-left: auto;
    }
    .cmd-input {
      background: #0f1218;
      border: 1px solid #2f3b4e;
      border-radius: 6px;
      padding: 5px 8px;
      color: #f1f5f9;
      font-size: 12px;
      font-family: monospace;
      width: 90px;
    }
    .cmd-input:focus {
      outline: none;
      border-color: #38bdf8;
    }

    /* Terminal window */
    .terminal-wrapper {
      background: #090d13;
      border: 1px solid #283141;
      border-radius: 10px;
      overflow: hidden;
      display: flex;
      flex-direction: column;
      flex: 1;
      min-height: 480px;
      box-shadow: 0 8px 24px rgba(0,0,0,0.5);
    }
    .terminal-header {
      background: #131720;
      padding: 8px 14px;
      border-bottom: 1px solid #222a38;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .terminal-dots {
      display: flex;
      gap: 6px;
    }
    .dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
    }
    .dot.red { background: #ef4444; }
    .dot.yellow { background: #eab308; }
    .dot.green { background: #22c55e; }
    .terminal-title {
      font-family: monospace;
      font-size: 12px;
      color: #64748b;
    }
    .terminal-bytes {
      font-family: monospace;
      font-size: 11px;
      color: #475569;
    }
    #terminalOutput {
      background: #090d13;
      color: #e2e8f0;
      font-family: "SF Mono", "Fira Code", Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
      font-size: 12.5px;
      line-height: 1.5;
      padding: 14px 16px;
      overflow-y: auto;
      overflow-x: auto;
      white-space: pre-wrap;
      word-break: break-all;
      flex: 1;
      min-height: 420px;
      max-height: 65vh;
    }
    #terminalOutput::-webkit-scrollbar {
      width: 8px;
      height: 8px;
    }
    #terminalOutput::-webkit-scrollbar-track {
      background: #090d13;
    }
    #terminalOutput::-webkit-scrollbar-thumb {
      background: #283141;
      border-radius: 4px;
    }

    footer {
      font-size: 12px;
      color: #64748b;
      text-align: center;
      padding: 4px 0 10px 0;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="header-left">
        <h1>🚐 CamperUI Web-Terminal</h1>
        <span class="badge" id="statBadgeVersion">Version wird geladen...</span>
        <span class="badge green" id="statBadgeStatus">Bereit</span>
      </div>
      <div style="display:flex;gap:8px;align-items:center;">
        <a href="/settings" class="nav-btn">⚙️ Einstellungen</a>
        <a href="/update" class="nav-btn">⚡ Update &rarr;</a>
      </div>
    </header>

    <!-- Stats Grid -->
    <div class="stats-grid">
      <div class="stat-card">
        <div class="stat-label">WLAN Verbindung</div>
        <div class="stat-val" id="statWifi">Verbinde...</div>
        <div class="stat-sub" id="statIp">IP: -</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">System Laufzeit</div>
        <div class="stat-val" id="statUptime">00:00:00</div>
        <div class="stat-sub">CPU: 240 MHz Dual-Core</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Speicher (Heap / PSRAM)</div>
        <div class="stat-val" id="statHeap">- KB frei</div>
        <div class="stat-sub" id="statPsram">- MB PSRAM</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">VanPi / Datenquelle</div>
        <div class="stat-val" id="statVanpi">Prüfe...</div>
        <div class="stat-sub" id="statVanpiSub">HTTP-Worker aktiv</div>
      </div>
    </div>

    <!-- Quick Commands -->
    <div class="cmd-bar">
      <span class="cmd-title">⚡ Befehle:</span>
      <button class="btn-cmd" onclick="sendCmd('w')">[w] WLAN & Speicher</button>
      <button class="btn-cmd" onclick="sendCmd('d')">[d] Demo / Live Modus</button>
      <button class="btn-cmd" onclick="sendCmd('b')">[b] Buzzer Test</button>
      <button class="btn-cmd" onclick="sendCmd('s')">[s] RGB Resync</button>
      <button class="btn-cmd" onclick="sendCmd('r')">[r] Redraw</button>
      <button class="btn-cmd" onclick="sendCmd('n')">[n] LVGL Health</button>
      <div class="cmd-input-wrap">
        <input type="text" id="cmdInput" class="cmd-input" placeholder="Befehl..." maxlength="8" onkeydown="if(event.key==='Enter') sendCustomCmd()">
        <button class="btn-cmd" onclick="sendCustomCmd()">Senden</button>
      </div>
    </div>

    <!-- Toolbar -->
    <div class="toolbar">
      <div class="btn-group">
        <button class="btn primary" id="btnCopy" onclick="copyLogs()">
          📋 In die Zwischenablage kopieren
        </button>
        <a href="/api/log?download=1" class="btn" download="camperui-debug.log">
          📥 Als Text speichern
        </a>
        <button class="btn danger" onclick="clearLogs()">
          🗑️ Leeren
        </button>
      </div>

      <div class="btn-group">
        <label class="toggle-label">
          <input type="checkbox" id="chkRefresh" checked onchange="autoRefresh = this.checked">
          Auto-Refresh (2s)
        </label>
        <label class="toggle-label">
          <input type="checkbox" id="chkScroll" checked onchange="autoScroll = this.checked">
          Auto-Scroll
        </label>
        <button class="btn" onclick="fetchLogs()">⟳ Aktualisieren</button>
      </div>
    </div>

    <!-- Terminal Output -->
    <div class="terminal-wrapper">
      <div class="terminal-header">
        <div class="terminal-dots">
          <div class="dot red"></div>
          <div class="dot yellow"></div>
          <div class="dot green"></div>
        </div>
        <div class="terminal-title">esp32s3-camper-ui ~ serial-debug-output</div>
        <div class="terminal-bytes" id="byteCount">0 KB</div>
      </div>
      <div id="terminalOutput">Lade Logs vom ESP32-S3...</div>
    </div>

    <footer>
      💡 Klicke auf <b>"In die Zwischenablage kopieren"</b>, um alle Logs direkt in einem Forum-Beitrag oder Support-Ticket zu teilen.
    </footer>
  </div>

  <script>
    let autoRefresh = true;
    let autoScroll = true;
    let isFetching = false;

    function copyLogs() {
      const text = document.getElementById('terminalOutput').textContent;
      if (!text) return;
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(() => {
          showCopySuccess();
        }).catch(() => {
          fallbackCopy(text);
        });
      } else {
        fallbackCopy(text);
      }
    }

    function showCopySuccess() {
      const btn = document.getElementById('btnCopy');
      const orig = btn.innerHTML;
      btn.innerHTML = '✅ In die Zwischenablage kopiert!';
      btn.classList.add('success');
      setTimeout(() => {
        btn.innerHTML = orig;
        btn.classList.remove('success');
      }, 2500);
    }

    function fallbackCopy(text) {
      const ta = document.createElement('textarea');
      ta.value = text;
      document.body.appendChild(ta);
      ta.select();
      try {
        document.execCommand('copy');
        showCopySuccess();
      } catch (e) {
        alert('Kopieren fehlgeschlagen. Bitte manuell markieren und kopieren.');
      }
      document.body.removeChild(ta);
    }

    function fetchLogs() {
      if (isFetching) return;
      isFetching = true;
      fetch('/api/log')
        .then(r => r.text())
        .then(text => {
          const el = document.getElementById('terminalOutput');
          const shouldScroll = autoScroll;
          el.textContent = text || '--- Keine Logs vorhanden ---';
          document.getElementById('byteCount').textContent = (text.length / 1024).toFixed(1) + ' KB';
          if (shouldScroll) {
            el.scrollTop = el.scrollHeight;
          }
        })
        .catch(() => {})
        .finally(() => { isFetching = false; });
    }

    function fetchStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(d => {
          if (d.version) {
            document.getElementById('statBadgeVersion').textContent = d.version;
          }
          if (d.uptime) {
            document.getElementById('statUptime').textContent = d.uptime;
          }
          if (d.wifi_ssid) {
            document.getElementById('statWifi').textContent = d.wifi_ssid + ' (' + d.wifi_rssi + ' dBm)';
            document.getElementById('statIp').textContent = 'IP: ' + d.wifi_ip;
          }
          if (d.heap_free !== undefined) {
            const heapKb = Math.round(d.heap_free / 1024);
            const psramMb = (d.psram_free / (1024 * 1024)).toFixed(1);
            document.getElementById('statHeap').textContent = heapKb + ' KB frei';
            document.getElementById('statPsram').textContent = psramMb + ' MB PSRAM';
          }
          if (d.vanpi_connected !== undefined) {
            const vpEl = document.getElementById('statVanpi');
            const vpSub = document.getElementById('statVanpiSub');
            if (d.debug_mode) {
              vpEl.textContent = 'Demo-Modus';
              vpEl.style.color = '#eab308';
              vpSub.textContent = 'Simulierte Sensordaten';
            } else if (d.vanpi_connected) {
              vpEl.textContent = 'Verbunden (Live)';
              vpEl.style.color = '#34d399';
              vpSub.textContent = 'VanPi HTTP-Daten aktiv';
            } else {
              vpEl.textContent = 'Nicht erreichbar';
              vpEl.style.color = '#f87171';
              vpSub.textContent = 'Warte auf VanPi...';
            }
          }
        })
        .catch(() => {});
    }

    function sendCmd(cmd) {
      fetch('/api/command?cmd=' + encodeURIComponent(cmd))
        .then(() => {
          setTimeout(fetchLogs, 300);
        })
        .catch(() => {});
    }

    function sendCustomCmd() {
      const input = document.getElementById('cmdInput');
      const val = input.value.trim();
      if (!val) return;
      sendCmd(val);
      input.value = '';
    }

    function clearLogs() {
      if (confirm('Moechtest du das Terminal-Log wirklich leeren?')) {
        fetch('/api/log?clear=1')
          .then(() => {
            document.getElementById('terminalOutput').textContent = '';
            document.getElementById('byteCount').textContent = '0 KB';
          })
          .catch(() => {});
      }
    }

    setInterval(() => {
      if (autoRefresh) fetchLogs();
    }, 2000);

    setInterval(fetchStatus, 4000);

    fetchStatus();
    fetchLogs();
  </script>
</body>
</html>
)rawliteral";

#endif // WEB_TERMINAL_HTML_H

