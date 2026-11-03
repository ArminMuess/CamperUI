#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VanPi Tailscale Gateway for CamperUI
====================================
Ermöglicht das Testen des CamperUI-Displays auf dem Schreibtisch, wenn das Display
nicht direkt im Camper-WLAN ist, sondern mit dem PC (Heim-WLAN oder PC-Hotspot) verbunden ist.
Der PC tunnelt alle Anfragen an das VanPi-System über Tailscale (z. B. 'pekaway' 100.80.161.23).

Features:
- Port 1880 Proxy (CamperUI-Standard) + Port 8088 Web-Dashboard
- Automatische Erkennung von Tailscale-Peers (z. B. 'pekaway') und PC-WLAN-IPs
- 3 Betriebsmodi:
    1. LIVE_FULL: Vollzugriff (alle GETs & PUTs an VanPi weitergeleitet)
    2. LIVE_SAFE: Sicherheits-Modus (Live-Sensoren lesen, Schaltbefehle blockieren & simulieren)
    3. OFFLINE:   Snapshot-/Replay-Modus (funktioniert auch wenn VanPi ausgeschaltet ist)
- Fast-Response Cache: Schützt das Display vor Latenzproblemen über Mobilfunk/Tailscale
- Live-Traffic-Monitor via SSE im Web-Dashboard
"""

import os
import sys
import json
import time
import socket
import select
import urllib.request
import urllib.error
import urllib.parse
import threading
import subprocess
import re
import argparse
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn

# Windows Console UTF-8 Safe Handling
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SNAPSHOT_FILE = os.path.join(SCRIPT_DIR, "camper_snapshot_default.json")
RUNTIME_SNAPSHOT_FILE = os.path.join(SCRIPT_DIR, "camper_snapshot.json")
CONFIG_FILE = os.path.join(SCRIPT_DIR, "gateway_config.json")

# ---------------------------------------------------------------------------
# State Management
# ---------------------------------------------------------------------------

class GatewayState:
    def __init__(self):
        self.lock = threading.Lock()
        self.mode = "live_safe"  # "live_full", "live_safe", "offline"
        self.target_host = "100.80.161.23"
        self.target_port = 1880
        self.vanpi_online = False
        self.vanpi_latency_ms = None
        self.last_vanpi_check = 0.0
        
        # Display tracking
        self.last_display_ip = None
        self.last_display_request_time = 0.0
        self.total_requests = 0
        
        # Cache / Snapshot
        self.data_cache = {}
        self.load_initial_snapshot()
        
        # SSE Event subscribers
        self.event_listeners = []
        self.recent_logs = []
        self.max_logs = 100
        
        # Fast cache worker enabled
        self.background_sync = True
        
        # Config loading
        self.load_config()

    def load_config(self):
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                    cfg = json.load(f)
                    self.mode = cfg.get("mode", self.mode)
                    self.target_host = cfg.get("target_host", self.target_host)
                    self.target_port = cfg.get("target_port", self.target_port)
                    self.background_sync = cfg.get("background_sync", self.background_sync)
            except Exception as e:
                print(f"[CONFIG] Fehler beim Laden von gateway_config.json: {e}")

    def save_config(self):
        try:
            with open(CONFIG_FILE, "w", encoding="utf-8") as f:
                json.dump({
                    "mode": self.mode,
                    "target_host": self.target_host,
                    "target_port": self.target_port,
                    "background_sync": self.background_sync
                }, f, indent=2)
        except Exception as e:
            print(f"[CONFIG] Fehler beim Speichern: {e}")

    def load_initial_snapshot(self):
        source = RUNTIME_SNAPSHOT_FILE if os.path.exists(RUNTIME_SNAPSHOT_FILE) else DEFAULT_SNAPSHOT_FILE
        if os.path.exists(source):
            try:
                with open(source, "r", encoding="utf-8") as f:
                    self.data_cache = json.load(f)
            except Exception as e:
                print(f"[SNAPSHOT] Fehler beim Laden: {e}")
        else:
            self.data_cache = {}

    def save_snapshot(self, filepath=None):
        target = filepath or RUNTIME_SNAPSHOT_FILE
        try:
            with self.lock:
                with open(target, "w", encoding="utf-8") as f:
                    json.dump(self.data_cache, f, indent=2, ensure_ascii=False)
            self.log_event("SNAPSHOT", f"Camper Snapshot gespeichert nach {os.path.basename(target)}")
            return True
        except Exception as e:
            self.log_event("ERROR", f"Snapshot-Speichern fehlgeschlagen: {e}")
            return False

    def log_event(self, category, message, details=None):
        event = {
            "time": time.strftime("%H:%M:%S"),
            "category": category,
            "message": message,
            "details": details or {}
        }
        with self.lock:
            self.recent_logs.append(event)
            if len(self.recent_logs) > self.max_logs:
                self.recent_logs.pop(0)
            dead = []
            for q in self.event_listeners:
                try:
                    q(event)
                except Exception:
                    dead.append(q)
            for d in dead:
                if d in self.event_listeners:
                    self.event_listeners.remove(d)

state = GatewayState()

# ---------------------------------------------------------------------------
# Network & Tailscale Utilities
# ---------------------------------------------------------------------------

def get_tailscale_peers():
    """Liest Tailscale Status aus und findet Peers wie 'pekaway'."""
    peers = []
    try:
        res = subprocess.run(["tailscale", "status", "--json"], capture_output=True, text=True, timeout=3)
        if res.returncode == 0:
            data = json.loads(res.stdout)
            for k, p in data.get("Peer", {}).items():
                name = p.get("HostName", "")
                ips = p.get("TailscaleIPs", [])
                online = p.get("Online", False)
                os_type = p.get("OS", "")
                if ips:
                    peers.append({
                        "name": name,
                        "ip": ips[0],
                        "all_ips": ips,
                        "online": online,
                        "os": os_type,
                        "is_likely_vanpi": ("pekaway" in name.lower() or "vanpi" in name.lower())
                    })
    except Exception:
        pass
    return peers

def get_local_ip_addresses():
    """Gibt alle lokalen IPv4-Adressen zurück."""
    results = []
    try:
        # Standard Socket Hostname Lookup
        hostname = socket.gethostname()
        for item in socket.getaddrinfo(hostname, None):
            ip = item[4][0]
            if ":" not in ip and not ip.startswith("127."):
                if ip not in [r["ip"] for r in results]:
                    label = "Netzwerk"
                    if ip.startswith("192.168."):
                        label = "WLAN / Heimnetzwerk"
                    elif ip.startswith("100."):
                        label = "Tailscale"
                    elif ip.startswith("172."):
                        label = "Virtuell / WSL"
                    elif ip.startswith("169.254."):
                        label = "Link-Local"
                    results.append({"ip": ip, "label": label})
    except Exception:
        pass

    # Sort: WLAN/Heimnetzwerk first, then others
    results.sort(key=lambda x: 0 if "WLAN" in x["label"] or "Heim" in x["label"] else (2 if "Tailscale" in x["label"] else 1))
    return results

def check_target_reachability(host, port, timeout=1.5):
    """Prüft TCP-Verbindung und HTTP-Health zum VanPi."""
    t0 = time.time()
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        err = sock.connect_ex((host, port))
        sock.close()
        latency = (time.time() - t0) * 1000.0
        return (err == 0), round(latency, 1)
    except Exception:
        return False, None

# ---------------------------------------------------------------------------
# Background Fast-Cache Sync Worker
# ---------------------------------------------------------------------------

VANPI_POLL_PATHS = [
    "/batt",
    "/mppt/",
    "/relay",
    "/wrelay",
    "/dimmer",
    "/level",
    "/temp",
    "/heater",
    "/position_sensor/?request=true",
    "/maxxfan/",
    "/info",
    "/shore",
    "/booster"
]

def background_sync_worker():
    """
    Hintergrund-Thread:
    Aktualisiert in regelmäßigen Abständen VanPi-Daten über Tailscale.
    Verhindert Display-Timeouts (CamperUI hat ein kurzes 800ms-Timeout!).
    """
    step = 0
    while True:
        try:
            time.sleep(0.4)
            if state.mode == "offline" or not state.background_sync:
                continue

            # Periodischer Health-Check alle 5 Sekunden
            now = time.time()
            if now - state.last_vanpi_check > 5.0:
                is_online, lat = check_target_reachability(state.target_host, state.target_port, timeout=1.5)
                state.vanpi_online = is_online
                state.vanpi_latency_ms = lat
                state.last_vanpi_check = now

            if not state.vanpi_online:
                continue

            path = VANPI_POLL_PATHS[step % len(VANPI_POLL_PATHS)]
            step += 1

            url = f"http://{state.target_host}:{state.target_port}{path}"
            req = urllib.request.Request(url, headers={"User-Agent": "CamperUIGateway/1.0"})
            with urllib.request.urlopen(req, timeout=2.0) as resp:
                if resp.status == 200:
                    raw = resp.read().decode("utf-8", errors="replace")
                    try:
                        clean_path = path.strip("/").split("?")[0]
                        if not clean_path:
                            clean_path = "mppt"
                        parsed = json.loads(raw)
                        with state.lock:
                            state.data_cache[clean_path] = parsed
                    except Exception:
                        pass
        except Exception:
            pass

# ---------------------------------------------------------------------------
# Safe-Mode Mock Command Handlers
# ---------------------------------------------------------------------------

def apply_safe_mock_command(path):
    """
    Simuliert Schaltbefehle lokal im Cache, damit das Display optisch sofort reagiert,
    ohne dass echte 12V-Relais, Dieselheizung oder Ventile im Camper geschaltet werden!
    """
    path = path.strip("/")
    
    # PUT /relay/<id>/<true|false>
    m = re.match(r"^relay/(\d+)/(true|false)$", path, re.IGNORECASE)
    if m:
        idx = int(m.group(1))
        val = m.group(2).lower() == "true"
        key = f"Relay{idx}"
        with state.lock:
            if "relay" not in state.data_cache:
                state.data_cache["relay"] = {}
            if key not in state.data_cache["relay"]:
                state.data_cache["relay"][key] = {"name": f"Relais {idx}", "state": val}
            else:
                state.data_cache["relay"][key]["state"] = val
        return True, f"Relais {idx} lokal auf {'EIN' if val else 'AUS'} gesetzt"

    # PUT /wrelay/<id>/<true|false>
    m = re.match(r"^wrelay/(\d+)/(true|false)$", path, re.IGNORECASE)
    if m:
        idx = int(m.group(1))
        val = m.group(2).lower() == "true"
        with state.lock:
            if "wrelay" not in state.data_cache:
                state.data_cache["wrelay"] = {}
            k1 = f"WifiRelay{idx}"
            k2 = f"wrelay{idx}"
            for k in (k1, k2):
                if k in state.data_cache["wrelay"]:
                    state.data_cache["wrelay"][k]["state"] = val
            if k1 not in state.data_cache["wrelay"] and k2 not in state.data_cache["wrelay"]:
                state.data_cache["wrelay"][k1] = {"name": f"WifiRelay {idx}", "state": val}
        return True, f"WRelay {idx} lokal auf {'EIN' if val else 'AUS'} gesetzt"

    # PUT /dimmer/<id>/<val>
    m = re.match(r"^dimmer/(\d+)/(\d+)$", path, re.IGNORECASE)
    if m:
        idx = int(m.group(1))
        val = max(0, min(100, int(m.group(2))))
        key = f"dimmer{idx}"
        with state.lock:
            if "dimmer" not in state.data_cache:
                state.data_cache["dimmer"] = {}
            if key not in state.data_cache["dimmer"]:
                state.data_cache["dimmer"][key] = {"name": f"Dimmer {idx}", "state": val}
            else:
                state.data_cache["dimmer"][key]["state"] = val
        return True, f"Dimmer {idx} lokal auf {val}% gesetzt"

    # PUT /autoterm/<mode>/<val>
    m = re.match(r"^autoterm/([a-zA-Z0-9_]+)/(\w+)$", path, re.IGNORECASE)
    if m:
        param = m.group(1).lower()
        val_str = m.group(2)
        with state.lock:
            if "heater" not in state.data_cache:
                state.data_cache["heater"] = {"autoterm1": {}}
            at = state.data_cache["heater"].setdefault("autoterm1", {})
            if param == "power":
                at["powerlevel"] = int(val_str)
                at["mode"] = "power"
                at["heatstatus"] = "Heizen (Stufe)"
            elif param == "temp":
                at["targettemp_vanpi"] = float(val_str)
                at["mode"] = "temp"
                at["heatstatus"] = "Heizen (Temp)"
            elif param == "fanspeed":
                at["fanspeed"] = int(val_str)
                at["mode"] = "vent"
                at["heatstatus"] = "Lüften"
            elif param == "toggle":
                at["heatertoggle"] = val_str.lower() in ("1", "true")
        return True, f"Heizung-Befehl {param}={val_str} lokal simuliert"

    # PUT /position_sensor/?request=calibrate
    if "position_sensor" in path and "calibrate" in path:
        with state.lock:
            state.data_cache["position_sensor"] = {"x_angle": 0.0, "y_angle": 0.0}
        return True, "Neigungssensor Nullpunkt tariert (lokal)"

    # PUT /maxxfan/<action>
    if path.startswith("maxxfan"):
        sub = path.split("/")
        with state.lock:
            if "maxxfan" not in state.data_cache:
                state.data_cache["maxxfan"] = {"maxxfan": {}}
            mf = state.data_cache["maxxfan"].setdefault("maxxfan", {})
            if len(sub) >= 2:
                action = sub[1]
                if action == "power":
                    mf["fan_power"] = not mf.get("fan_power", False)
                elif action == "auto":
                    mf["fan_auto"] = not mf.get("fan_auto", False)
                elif action == "vent":
                    mf["fan_vent"] = "close" if mf.get("fan_vent") == "open" else "open"
                elif action == "speed" and len(sub) >= 3:
                    mf["fan_speed"] = int(sub[2])
                elif action == "temp" and len(sub) >= 3:
                    mf["fan_temp"] = int(sub[2])
        return True, f"MaxxFan Befehl '{path}' lokal simuliert"

    return False, f"Unbekannter Befehl: {path}"

# ---------------------------------------------------------------------------
# HTTP Request Handlers
# ---------------------------------------------------------------------------

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True

class VanPiProxyHandler(BaseHTTPRequestHandler):
    """
    Behandelt Anfragen vom CamperUI-Display auf Port 1880.
    """
    server_version = "CamperUIGateway/1.0"

    def log_message(self, format, *args):
        # Wir loggen über unsere eigene strukturierte Routine
        pass

    def do_GET(self):
        client_ip = self.client_address[0]
        state.last_display_ip = client_ip
        state.last_display_request_time = time.time()
        state.total_requests += 1

        # Falls ein Webbrowser auf Port 1880 zugreift (z.B. http://192.168.1.170:1880/):
        accept = self.headers.get("Accept", "")
        if self.path in ("/", "/_gateway", "/gateway", "/index.html") and "text/html" in accept:
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(get_dashboard_html().encode("utf-8"))
            return

        clean_path = self.path.strip("/").split("?")[0]
        if not clean_path and "mppt" in self.path:
            clean_path = "mppt"

        t0 = time.time()
        source_label = "OFFLINE"

        # 1. Modus: OFFLINE oder VanPi ist offline -> Direkt aus Snapshot/Cache liefern
        if state.mode == "offline" or (state.mode in ("live_safe", "live_full") and not state.vanpi_online and clean_path in state.data_cache):
            if clean_path in state.data_cache:
                payload = json.dumps(state.data_cache[clean_path], ensure_ascii=False)
                dur = round((time.time() - t0) * 1000, 1)
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("X-Gateway-Mode", state.mode)
                self.end_headers()
                self.wfile.write(payload.encode("utf-8"))
                state.log_event("PROXY_GET", f"GET {self.path} -> 200 OK ({dur}ms)", {
                    "client": client_ip, "mode": "OFFLINE_CACHE", "status": 200, "ms": dur
                })
                print(f"\033[96m[DISP {client_ip}]\033[0m GET {self.path} -> \033[93m200 OK (Cache {dur}ms)\033[0m")
                return

        # 2. Modus: Fast-Cache Aktiv & Daten vorhanden -> Direkt aus Cache antworten (Blitzschnell für Display!)
        if state.background_sync and clean_path in state.data_cache:
            payload = json.dumps(state.data_cache[clean_path], ensure_ascii=False)
            dur = round((time.time() - t0) * 1000, 1)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("X-Gateway-Mode", state.mode)
            self.end_headers()
            self.wfile.write(payload.encode("utf-8"))
            state.log_event("PROXY_GET", f"GET {self.path} -> 200 OK ({dur}ms)", {
                "client": client_ip, "mode": "FAST_CACHE", "status": 200, "ms": dur
            })
            return

        # 3. Live Pass-Through: Anfrage via Tailscale an VanPi leiten
        target_url = f"http://{state.target_host}:{state.target_port}{self.path}"
        try:
            req = urllib.request.Request(target_url, headers={
                "User-Agent": "CamperUIGateway/1.0",
                "Accept": self.headers.get("Accept", "*/*")
            })
            with urllib.request.urlopen(req, timeout=1.8) as resp:
                data = resp.read()
                code = resp.status
                dur = round((time.time() - t0) * 1000, 1)
                
                self.send_response(code)
                for h, v in resp.getheaders():
                    if h.lower() not in ("server", "date", "transfer-encoding", "content-length"):
                        self.send_header(h, v)
                self.send_header("Content-Length", str(len(data)))
                self.send_header("X-Gateway-Mode", state.mode)
                self.end_headers()
                self.wfile.write(data)

                # Cache aktualisieren
                try:
                    js = json.loads(data.decode("utf-8", errors="ignore"))
                    with state.lock:
                        state.data_cache[clean_path] = js
                except Exception:
                    pass

                state.log_event("PROXY_GET", f"GET {self.path} -> {code} ({dur}ms via Tailscale)", {
                    "client": client_ip, "mode": state.mode, "status": code, "ms": dur
                })
                print(f"\033[96m[DISP {client_ip}]\033[0m GET {self.path} -> \033[92m{code} ({dur}ms Tailscale)\033[0m")

        except Exception as e:
            # Fallback auf Cache falls vorhanden
            dur = round((time.time() - t0) * 1000, 1)
            if clean_path in state.data_cache:
                payload = json.dumps(state.data_cache[clean_path], ensure_ascii=False)
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("X-Gateway-Fallback", "true")
                self.end_headers()
                self.wfile.write(payload.encode("utf-8"))
                state.log_event("PROXY_FALLBACK", f"GET {self.path} -> Fallback auf Cache ({e})", {
                    "client": client_ip, "status": 200, "ms": dur
                })
                print(f"\033[96m[DISP {client_ip}]\033[0m GET {self.path} -> \033[93mFallback Cache ({dur}ms)\033[0m")
            else:
                self.send_response(502)
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                self.wfile.write(f"Gateway Fehler: {e}".encode("utf-8"))
                state.log_event("ERROR", f"GET {self.path} -> 502 Bad Gateway ({e})", {
                    "client": client_ip, "status": 502, "ms": dur
                })
                print(f"\033[96m[DISP {client_ip}]\033[0m GET {self.path} -> \033[91m502 Bad Gateway ({e})\033[0m")

    def do_PUT(self):
        client_ip = self.client_address[0]
        state.last_display_ip = client_ip
        state.last_display_request_time = time.time()
        state.total_requests += 1

        t0 = time.time()
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length) if content_length > 0 else b""

        # =====================================================================
        # SAFE MODE & OFFLINE MODE: Schaltbefehle abfangen und lokal simulieren!
        # =====================================================================
        if state.mode in ("live_safe", "offline"):
            success, msg = apply_safe_mock_command(self.path)
            dur = round((time.time() - t0) * 1000, 1)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("X-Gateway-SafeMode", "simulated")
            self.end_headers()
            self.wfile.write(json.dumps({"status": "ok", "simulated": True, "message": msg}).encode("utf-8"))

            badge = "SAFE-MODUS (Camper geschützt)" if state.mode == "live_safe" else "OFFLINE-MODUS"
            state.log_event("COMMAND_SAFE", f"PUT {self.path} -> {msg}", {
                "client": client_ip, "mode": state.mode, "status": 200, "action": msg, "ms": dur
            })
            print(f"\033[96m[DISP {client_ip}]\033[0m PUT {self.path} -> \033[93m[{badge}] {msg}\033[0m")
            return

        # =====================================================================
        # LIVE_FULL MODE: Echte Hardware im Camper über Tailscale schalten!
        # =====================================================================
        target_url = f"http://{state.target_host}:{state.target_port}{self.path}"
        try:
            req = urllib.request.Request(target_url, data=body, method="PUT", headers={
                "User-Agent": "CamperUIGateway/1.0",
                "Content-Type": self.headers.get("Content-Type", "application/json")
            })
            with urllib.request.urlopen(req, timeout=2.5) as resp:
                resp_data = resp.read()
                code = resp.status
                dur = round((time.time() - t0) * 1000, 1)
                
                self.send_response(code)
                self.send_header("Content-Type", resp.headers.get("Content-Type", "application/json"))
                self.end_headers()
                self.wfile.write(resp_data)

                # Den Safe-Mock-State trotzdem im Cache spiegeln für sofortiges Update
                apply_safe_mock_command(self.path)

                state.log_event("COMMAND_LIVE", f"PUT {self.path} an VanPi gesendet -> {code} ({dur}ms)", {
                    "client": client_ip, "mode": "LIVE_FULL", "status": code, "ms": dur
                })
                print(f"\033[96m[DISP {client_ip}]\033[0m PUT {self.path} -> \033[92m{code} OK (VanPi geschaltet via Tailscale!)\033[0m")

        except Exception as e:
            dur = round((time.time() - t0) * 1000, 1)
            self.send_response(502)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(f"Fehler beim Senden an VanPi: {e}".encode("utf-8"))
            state.log_event("ERROR", f"PUT {self.path} fehlgeschlagen: {e}", {
                "client": client_ip, "status": 502, "ms": dur
            })
            print(f"\033[96m[DISP {client_ip}]\033[0m PUT {self.path} -> \033[91m502 Fehler ({e})\033[0m")

    def do_POST(self):
        # Falls CamperUI oder MaxxFan POST verwendet
        self.do_PUT()


# ---------------------------------------------------------------------------
# Dedicated Web Dashboard & Management API Handler (Port 8088)
# ---------------------------------------------------------------------------

class DashboardAPIHandler(BaseHTTPRequestHandler):
    server_version = "CamperUIGatewayDashboard/1.0"

    def log_message(self, format, *args):
        pass

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path in ("/", "/index.html"):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(get_dashboard_html().encode("utf-8"))
            return

        if path == "/api/status":
            local_ips = get_local_ip_addresses()
            ts_peers = get_tailscale_peers()
            
            with state.lock:
                disp_online = (time.time() - state.last_display_request_time) < 5.0 if state.last_display_request_time > 0 else False
                resp = {
                    "mode": state.mode,
                    "target_host": state.target_host,
                    "target_port": state.target_port,
                    "vanpi_online": state.vanpi_online,
                    "vanpi_latency_ms": state.vanpi_latency_ms,
                    "display_ip": state.last_display_ip,
                    "display_online": disp_online,
                    "display_last_seen": round(time.time() - state.last_display_request_time, 1) if state.last_display_request_time > 0 else None,
                    "total_requests": state.total_requests,
                    "local_ips": local_ips,
                    "tailscale_peers": ts_peers,
                    "background_sync": state.background_sync,
                    "cache_keys": list(state.data_cache.keys())
                }
            self.send_json(resp)
            return

        if path == "/api/data":
            with state.lock:
                copy_data = dict(state.data_cache)
            self.send_json(copy_data)
            return

        if path == "/api/events":
            # Server-Sent Events (SSE) für Live-Log-Streaming
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()

            queue = []
            q_lock = threading.Lock()
            cond = threading.Condition(q_lock)

            def listener(evt):
                with cond:
                    queue.append(evt)
                    cond.notify()

            with state.lock:
                state.event_listeners.append(listener)
                # Zunächst die letzten 15 Events senden
                for log in state.recent_logs[-15:]:
                    queue.append(log)

            try:
                while True:
                    with cond:
                        while not queue:
                            cond.wait(timeout=2.0)
                            if not queue:
                                # Keepalive ping
                                self.wfile.write(b": keepalive\n\n")
                                self.wfile.flush()
                        events_to_send = list(queue)
                        queue.clear()

                    for evt in events_to_send:
                        line = f"data: {json.dumps(evt, ensure_ascii=False)}\n\n"
                        self.wfile.write(line.encode("utf-8"))
                    self.wfile.flush()
            except Exception:
                with state.lock:
                    if listener in state.event_listeners:
                        state.event_listeners.remove(listener)
            return

        if path == "/api/test":
            # Test-Request an VanPi auslösen
            qs = urllib.parse.parse_qs(parsed.query)
            ep = qs.get("endpoint", ["/batt"])[0]
            if not ep.startswith("/"):
                ep = "/" + ep
            url = f"http://{state.target_host}:{state.target_port}{ep}"
            t0 = time.time()
            try:
                req = urllib.request.Request(url, headers={"User-Agent": "CamperUIGateway/1.0"})
                with urllib.request.urlopen(req, timeout=2.5) as r:
                    dur = round((time.time() - t0) * 1000, 1)
                    raw = r.read().decode("utf-8", errors="replace")
                    self.send_json({"status": "ok", "code": r.status, "ms": dur, "data": raw})
            except Exception as e:
                dur = round((time.time() - t0) * 1000, 1)
                self.send_json({"status": "error", "error": str(e), "ms": dur}, status=502)
            return

        self.send_response(404)
        self.end_headers()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode("utf-8") if length > 0 else "{}"
        try:
            req_data = json.loads(body)
        except Exception:
            req_data = {}

        if path == "/api/mode":
            new_mode = req_data.get("mode")
            if new_mode in ("live_full", "live_safe", "offline"):
                state.mode = new_mode
                state.save_config()
                mode_names = {
                    "live_full": "⚡ LIVE VOLLZUGRIFF (Echtes Schalten via Tailscale)",
                    "live_safe": "🛡️ LIVE SAFE-MODUS (Live-Lesen, Befehle lokal simuliert)",
                    "offline":   "💾 OFFLINE-MODUS (Lokaler Snapshot/Demo)"
                }
                state.log_event("MODE_CHANGE", f"Modus gewechselt zu: {mode_names.get(new_mode, new_mode)}")
                self.send_json({"status": "ok", "mode": state.mode})
                return
            self.send_json({"status": "error", "message": "Ungültiger Modus"}, status=400)
            return

        if path == "/api/target":
            host = req_data.get("target_host")
            port = int(req_data.get("target_port", 1880))
            if host:
                state.target_host = host.strip()
                state.target_port = port
                state.save_config()
                state.last_vanpi_check = 0.0  # Sofort neu prüfen
                state.log_event("TARGET_CHANGE", f"VanPi Ziel geändert zu: {state.target_host}:{state.target_port}")
                self.send_json({"status": "ok", "target_host": state.target_host, "target_port": state.target_port})
                return
            self.send_json({"status": "error", "message": "Fehlender Host"}, status=400)
            return

        if path == "/api/snapshot/save":
            success = state.save_snapshot()
            self.send_json({"status": "ok" if success else "error"})
            return

        if path == "/api/background_sync":
            val = bool(req_data.get("enabled", True))
            state.background_sync = val
            state.save_config()
            state.log_event("CONFIG", f"Fast-Cache Background-Sync: {'Aktiv' if val else 'Deaktiviert'}")
            self.send_json({"status": "ok", "background_sync": state.background_sync})
            return

        self.send_response(404)
        self.end_headers()

    def send_json(self, data, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(data, ensure_ascii=False, indent=2).encode("utf-8"))


# ---------------------------------------------------------------------------
# Embedded Web Dashboard Single-Page App
# ---------------------------------------------------------------------------

def get_dashboard_html():
    return """<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CamperUI ↔ VanPi Tailscale Gateway</title>
<style>
:root {
  --bg: #0d1117;
  --panel: #161b22;
  --panel-border: #30363d;
  --text: #e6edf3;
  --text-muted: #8b949e;
  --accent-cyan: #38bdf8;
  --accent-green: #2ea043;
  --accent-yellow: #d29922;
  --accent-red: #f85149;
  --accent-blue: #58a6ff;
  --font-mono: 'SFMono-Regular', Consolas, 'Liberation Mono', Menlo, Courier, monospace;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background-color: var(--bg);
  color: var(--text);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  padding: 20px;
  line-height: 1.5;
}
.container { max-width: 1200px; margin: 0 auto; }
header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 24px;
  padding-bottom: 16px;
  border-bottom: 1px solid var(--panel-border);
}
.header-title { display: flex; align-items: center; gap: 12px; }
.header-title h1 { font-size: 22px; font-weight: 600; }
.badge {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 4px 10px;
  border-radius: 9999px;
  font-size: 12px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}
.badge-green { background: rgba(46, 160, 67, 0.2); color: #3fb950; border: 1px solid rgba(46, 160, 67, 0.4); }
.badge-yellow { background: rgba(210, 153, 34, 0.2); color: #e3b341; border: 1px solid rgba(210, 153, 34, 0.4); }
.badge-red { background: rgba(248, 81, 73, 0.2); color: #f85149; border: 1px solid rgba(248, 81, 73, 0.4); }
.badge-blue { background: rgba(88, 166, 255, 0.2); color: #58a6ff; border: 1px solid rgba(88, 166, 255, 0.4); }

.instruction-hero {
  background: linear-gradient(135deg, #132338 0%, #161b22 100%);
  border: 2px solid var(--accent-cyan);
  border-radius: 12px;
  padding: 20px 24px;
  margin-bottom: 24px;
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.3);
}
.instruction-hero h2 {
  font-size: 16px;
  color: var(--accent-cyan);
  margin-bottom: 8px;
  display: flex;
  align-items: center;
  gap: 8px;
}
.ip-showcase {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 16px;
  margin-top: 12px;
  background: rgba(0,0,0,0.3);
  padding: 12px 16px;
  border-radius: 8px;
}
.ip-tag {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 18px;
  font-weight: 700;
  font-family: var(--font-mono);
  color: #fff;
}
.btn-copy {
  background: rgba(56, 189, 248, 0.15);
  border: 1px solid var(--accent-cyan);
  color: var(--accent-cyan);
  padding: 5px 12px;
  border-radius: 6px;
  font-size: 13px;
  cursor: pointer;
  transition: all 0.2s;
}
.btn-copy:hover { background: var(--accent-cyan); color: #000; }

.grid-3 {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  gap: 20px;
  margin-bottom: 24px;
}
.card {
  background: var(--panel);
  border: 1px solid var(--panel-border);
  border-radius: 10px;
  padding: 18px;
}
.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 14px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(255,255,255,0.06);
}
.card-header h3 { font-size: 15px; font-weight: 600; }

.mode-selector {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
.mode-option {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  padding: 12px;
  border-radius: 8px;
  border: 1px solid var(--panel-border);
  background: rgba(0,0,0,0.2);
  cursor: pointer;
  transition: all 0.2s;
}
.mode-option:hover { border-color: rgba(255,255,255,0.3); }
.mode-option.active-safe {
  border-color: var(--accent-yellow);
  background: rgba(210, 153, 34, 0.12);
}
.mode-option.active-live {
  border-color: var(--accent-green);
  background: rgba(46, 160, 67, 0.12);
}
.mode-option.active-offline {
  border-color: var(--accent-blue);
  background: rgba(88, 166, 255, 0.12);
}
.mode-text h4 { font-size: 14px; margin-bottom: 2px; }
.mode-text p { font-size: 12px; color: var(--text-muted); line-height: 1.3; }

.form-group { margin-bottom: 12px; }
.form-group label { display: block; font-size: 12px; color: var(--text-muted); margin-bottom: 4px; }
.form-control {
  width: 100%;
  padding: 8px 12px;
  background: #0d1117;
  border: 1px solid var(--panel-border);
  border-radius: 6px;
  color: #fff;
  font-family: var(--font-mono);
  font-size: 13px;
}
.form-control:focus { outline: none; border-color: var(--accent-cyan); }
.btn {
  padding: 7px 14px;
  border-radius: 6px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  border: 1px solid transparent;
  transition: all 0.2s;
}
.btn-primary { background: #238636; color: #fff; }
.btn-primary:hover { background: #2ea043; }
.btn-secondary { background: #21262d; border-color: var(--panel-border); color: #c9d1d9; }
.btn-secondary:hover { background: #30363d; }

.tabs {
  display: flex;
  gap: 8px;
  border-bottom: 1px solid var(--panel-border);
  margin-bottom: 16px;
}
.tab-btn {
  padding: 8px 16px;
  background: transparent;
  border: none;
  color: var(--text-muted);
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
  border-bottom: 2px solid transparent;
}
.tab-btn.active { color: var(--accent-cyan); border-bottom-color: var(--accent-cyan); }

.log-table-wrapper {
  background: #090d13;
  border: 1px solid var(--panel-border);
  border-radius: 8px;
  overflow: hidden;
}
.log-table {
  width: 100%;
  border-collapse: collapse;
  font-family: var(--font-mono);
  font-size: 12px;
}
.log-table th {
  background: #161b22;
  text-align: left;
  padding: 8px 12px;
  color: var(--text-muted);
  font-size: 11px;
  text-transform: uppercase;
}
.log-table td {
  padding: 6px 12px;
  border-bottom: 1px solid rgba(255,255,255,0.04);
}
.log-table tr:hover { background: rgba(255,255,255,0.02); }
.method-get { color: var(--accent-cyan); font-weight: bold; }
.method-put { color: var(--accent-yellow); font-weight: bold; }
.status-200 { color: #3fb950; }
.status-err { color: #f85149; }

.data-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  gap: 16px;
}
.data-card {
  background: #0d1117;
  border: 1px solid var(--panel-border);
  border-radius: 8px;
  padding: 14px;
}
.data-card h4 { font-size: 13px; color: var(--text-muted); margin-bottom: 8px; }
.data-val { font-size: 20px; font-weight: bold; font-family: var(--font-mono); }
.data-val-sub { font-size: 12px; color: var(--text-muted); margin-top: 4px; }
</style>
</head>
<body>

<div class="container">
  <header>
    <div class="header-title">
      <h1>🚐 CamperUI ↔ VanPi Gateway</h1>
      <span id="gatewayModeBadge" class="badge badge-yellow">Safe Mode</span>
    </div>
    <div style="display: flex; gap: 10px; align-items: center;">
      <span id="displayBadge" class="badge badge-red">Display nicht verbunden</span>
      <span id="vanpiBadge" class="badge badge-blue">VanPi Status...</span>
    </div>
  </header>

  <!-- Hero Banner: How to connect the display -->
  <div class="instruction-hero">
    <h2>📡 Schritt 1: So verbindest du dein CamperUI Display</h2>
    <p style="font-size: 14px; color: #c9d1d9;">
      1. Verbinde das Display über WLAN mit demselben Netzwerk wie deinen PC (oder mit dem PC-Hotspot).<br>
      2. Öffne auf dem Display: <strong>Einstellungen (Zahnrad) → Netzwerk</strong>.<br>
      3. Trage als <strong>VanPi-IP</strong> die folgende IP deines PCs ein:
    </p>
    <div class="ip-showcase">
      <div class="ip-tag">
        <span style="color: var(--accent-cyan);">IP:</span>
        <span id="primaryLocalIp">Lade...</span>
      </div>
      <button class="btn-copy" onclick="copyIp()">📋 In Zwischenablage kopieren</button>
      <span style="font-size: 12px; color: var(--text-muted);">
        (Port <strong>1880</strong> ist fest im Display hinterlegt – das Gateway lauscht genau darauf!)
      </span>
    </div>
  </div>

  <div class="grid-3">
    <!-- Card 1: Operation Mode -->
    <div class="card">
      <div class="card-header">
        <h3>🛡️ Betriebsmodus & Schutz</h3>
      </div>
      <div class="mode-selector">
        <div id="modeSafe" class="mode-option active-safe" onclick="setMode('live_safe')">
          <input type="radio" name="mode" checked>
          <div class="mode-text">
            <h4 style="color: #e3b341;">SAFE-Modus (Empfohlen)</h4>
            <p>Live-Sensoren aus dem Camper lesen (Batterie, Solar, Temp). Schaltbefehle (Heizung, Licht) werden lokal abgefangen & simuliert, um die echte Hardware im Camper zu schützen!</p>
          </div>
        </div>
        <div id="modeLive" class="mode-option" onclick="setMode('live_full')">
          <input type="radio" name="mode">
          <div class="mode-text">
            <h4 style="color: #3fb950;">LIVE-Modus (Vollzugriff)</h4>
            <p>Echte 1:1 Verbindung! Klicks auf dem Display schalten tatsächliche Relais, Heizung & Ventile im Fahrzeug über Tailscale.</p>
          </div>
        </div>
        <div id="modeOffline" class="mode-option" onclick="setMode('offline')">
          <input type="radio" name="mode">
          <div class="mode-text">
            <h4 style="color: #58a6ff;">OFFLINE-Modus (Replay)</h4>
            <p>Verwendet den gespeicherten Snapshot. Perfekt zum Entwickeln, wenn der Camper ausgeschaltet oder unterwegs kein Netz vorhanden ist.</p>
          </div>
        </div>
      </div>
    </div>

    <!-- Card 2: VanPi Tailscale Target -->
    <div class="card">
      <div class="card-header">
        <h3>🚐 VanPi Ziel (Tailscale)</h3>
        <span id="vanpiPingStatus" style="font-size: 12px; font-family: var(--font-mono); color: var(--text-muted);">-- ms</span>
      </div>
      <div class="form-group">
        <label>Tailscale Peers erkannt:</label>
        <select id="peerSelect" class="form-control" onchange="selectPeer(this.value)">
          <option value="">Lade Peers...</option>
        </select>
      </div>
      <div class="form-group">
        <label>VanPi IP / Hostname:</label>
        <input type="text" id="targetHostInput" class="form-control" value="100.80.161.23">
      </div>
      <div style="display: flex; gap: 8px;">
        <button class="btn btn-primary" onclick="saveTarget()" style="flex: 1;">Ziel Speichern</button>
        <button class="btn btn-secondary" onclick="testVanPi()">Test-GET</button>
      </div>
      <div style="margin-top: 14px; font-size: 12px; color: var(--text-muted);">
        <input type="checkbox" id="chkFastCache" checked onchange="toggleFastCache(this.checked)">
        <label for="chkFastCache" style="display: inline; margin-left: 4px; cursor: pointer;">
          Fast-Cache aktivieren (beugt Display 800ms-Timeout vor)
        </label>
      </div>
    </div>

    <!-- Card 3: Display Status -->
    <div class="card">
      <div class="card-header">
        <h3>📱 CamperUI Display</h3>
        <span id="displayReqCount" style="font-size: 12px; color: var(--text-muted);">0 Requests</span>
      </div>
      <div style="padding: 10px 0;">
        <div style="margin-bottom: 8px; font-size: 13px;">
          <span style="color: var(--text-muted);">Display-IP:</span> 
          <strong id="dispIpVal" style="font-family: var(--font-mono); color: #fff;">Noch keine Anfrage</strong>
        </div>
        <div style="margin-bottom: 8px; font-size: 13px;">
          <span style="color: var(--text-muted);">Status:</span> 
          <span id="dispStatusText">Wartet auf Display...</span>
        </div>
        <div style="font-size: 13px;">
          <span style="color: var(--text-muted);">Letzte Aktivität:</span> 
          <span id="dispLastSeen">--</span>
        </div>
      </div>
      <div style="margin-top: 16px; border-top: 1px solid var(--panel-border); padding-top: 12px;">
        <button class="btn btn-secondary" onclick="saveSnapshotNow()" style="width: 100%;">
          📸 Aktuellen Camper-Zustand als Snapshot sichern
        </button>
      </div>
    </div>
  </div>

  <!-- Tabs: Live Logs vs. Camper Data Inspector -->
  <div class="tabs">
    <button id="tabLogsBtn" class="tab-btn active" onclick="switchTab('logs')">Live Traffic Monitor</button>
    <button id="tabDataBtn" class="tab-btn" onclick="switchTab('data')">Camper Live-Werte (JSON)</button>
  </div>

  <!-- Tab 1: Live Logs -->
  <div id="tabLogs">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
      <span style="font-size: 13px; color: var(--text-muted);">Echtzeit-Stream aller Anfragen vom CamperUI-Display:</span>
      <button class="btn btn-secondary" onclick="clearLogs()" style="padding: 3px 8px; font-size: 11px;">Logs leeren</button>
    </div>
    <div class="log-table-wrapper" style="max-height: 400px; overflow-y: auto;">
      <table class="log-table">
        <thead>
          <tr>
            <th style="width: 80px;">Zeit</th>
            <th style="width: 120px;">Client</th>
            <th>Anfrage / Aktion</th>
            <th style="width: 130px;">Modus</th>
            <th style="width: 70px;">Latenz</th>
          </tr>
        </thead>
        <tbody id="logTableBody">
          <tr><td colspan="5" style="color: var(--text-muted); text-align: center; padding: 20px;">Warte auf Anfragen vom Display...</td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <!-- Tab 2: Live Camper Data -->
  <div id="tabData" style="display: none;">
    <div class="data-grid" id="dataCardsContainer">
      <div class="data-card"><h4>Lade Daten...</h4></div>
    </div>
  </div>

</div>

<script>
let currentPrimaryIp = "";

function copyIp() {
  if (currentPrimaryIp) {
    navigator.clipboard.writeText(currentPrimaryIp).then(() => {
      alert("IP " + currentPrimaryIp + " wurde kopiert!\\nTrage diese im CamperUI Display unter Einstellungen -> Netzwerk ein.");
    });
  }
}

function updateStatus() {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      // Mode Badge & Radio
      const mb = document.getElementById('gatewayModeBadge');
      document.querySelectorAll('.mode-option').forEach(el => el.classList.remove('active-safe', 'active-live', 'active-offline'));
      if (data.mode === 'live_safe') {
        mb.className = 'badge badge-yellow';
        mb.textContent = 'Safe Mode';
        document.getElementById('modeSafe').classList.add('active-safe');
      } else if (data.mode === 'live_full') {
        mb.className = 'badge badge-green';
        mb.textContent = 'Live Vollzugriff';
        document.getElementById('modeLive').classList.add('active-live');
      } else {
        mb.className = 'badge badge-blue';
        mb.textContent = 'Offline Replay';
        document.getElementById('modeOffline').classList.add('active-offline');
      }

      // Local IP
      if (data.local_ips && data.local_ips.length > 0) {
        currentPrimaryIp = data.local_ips[0].ip;
        document.getElementById('primaryLocalIp').textContent = currentPrimaryIp + " (" + data.local_ips[0].label + ")";
      }

      // VanPi Badge
      const vb = document.getElementById('vanpiBadge');
      const ping = document.getElementById('vanpiPingStatus');
      if (data.vanpi_online) {
        vb.className = 'badge badge-green';
        vb.textContent = 'VanPi Online (' + (data.vanpi_latency_ms || '?') + 'ms)';
        ping.textContent = (data.vanpi_latency_ms || '?') + ' ms via Tailscale';
        ping.style.color = '#3fb950';
      } else {
        vb.className = 'badge badge-red';
        vb.textContent = 'VanPi Offline';
        ping.textContent = 'Nicht erreichbar';
        ping.style.color = '#f85149';
      }

      // Display Status
      const db = document.getElementById('displayBadge');
      document.getElementById('displayReqCount').textContent = data.total_requests + " Requests";
      if (data.display_ip) {
        document.getElementById('dispIpVal').textContent = data.display_ip;
      }
      if (data.display_online) {
        db.className = 'badge badge-green';
        db.textContent = 'Display Aktiv';
        document.getElementById('dispStatusText').innerHTML = '<span style="color: #3fb950;">Verbunden & pollt</span>';
        document.getElementById('dispLastSeen').textContent = 'vor ' + data.display_last_seen + 's';
      } else if (data.display_last_seen !== null) {
        db.className = 'badge badge-yellow';
        db.textContent = 'Display Inaktiv';
        document.getElementById('dispStatusText').textContent = 'Letzte Verbindung vor ' + data.display_last_seen + 's';
        document.getElementById('dispLastSeen').textContent = 'vor ' + data.display_last_seen + 's';
      } else {
        db.className = 'badge badge-red';
        db.textContent = 'Wartet auf Display';
        document.getElementById('dispStatusText').textContent = 'Noch keine Verbindung';
      }

      // Tailscale Peers Select
      const sel = document.getElementById('peerSelect');
      if (data.tailscale_peers && data.tailscale_peers.length > 0) {
        let opts = '<option value="">-- Peer auswählen --</option>';
        data.tailscale_peers.forEach(p => {
          const star = p.is_likely_vanpi ? ' ⭐ (VanPi)' : '';
          const onl = p.online ? '🟢' : '⚪';
          opts += `<option value="${p.ip}">${onl} ${p.name} (${p.ip})${star}</option>`;
        });
        sel.innerHTML = opts;
      }

      document.getElementById('chkFastCache').checked = data.background_sync;
    })
    .catch(() => {});
}

function setMode(mode) {
  fetch('/api/mode', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mode: mode })
  }).then(() => updateStatus());
}

function saveTarget() {
  const host = document.getElementById('targetHostInput').value;
  fetch('/api/target', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ target_host: host, target_port: 1880 })
  }).then(r => r.json()).then(res => {
    alert("Ziel aktualisiert!");
    updateStatus();
  });
}

function selectPeer(ip) {
  if (ip) {
    document.getElementById('targetHostInput').value = ip;
    saveTarget();
  }
}

function testVanPi() {
  fetch('/api/test?endpoint=/batt')
    .then(r => r.json())
    .then(data => {
      if (data.status === 'ok') {
        alert("Erfolg! /batt erhalten (" + data.ms + "ms):\\n" + data.data);
      } else {
        alert("Fehler (" + data.ms + "ms):\\n" + data.error);
      }
      updateStatus();
    });
}

function saveSnapshotNow() {
  fetch('/api/snapshot/save', { method: 'POST' })
    .then(r => r.json())
    .then(data => {
      alert(data.status === 'ok' ? "Snapshot erfolgreich gespeichert!" : "Fehler beim Speichern!");
    });
}

function toggleFastCache(val) {
  fetch('/api/background_sync', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ enabled: val })
  });
}

function clearLogs() {
  document.getElementById('logTableBody').innerHTML = '<tr><td colspan="5" style="color: var(--text-muted); text-align: center; padding: 20px;">Logs geleert</td></tr>';
}

function switchTab(t) {
  if (t === 'logs') {
    document.getElementById('tabLogs').style.display = 'block';
    document.getElementById('tabData').style.display = 'none';
    document.getElementById('tabLogsBtn').classList.add('active');
    document.getElementById('tabDataBtn').classList.remove('active');
  } else {
    document.getElementById('tabLogs').style.display = 'none';
    document.getElementById('tabData').style.display = 'block';
    document.getElementById('tabLogsBtn').classList.remove('active');
    document.getElementById('tabDataBtn').classList.add('active');
    loadDataInspector();
  }
}

function loadDataInspector() {
  fetch('/api/data')
    .then(r => r.json())
    .then(data => {
      const container = document.getElementById('dataCardsContainer');
      let html = '';
      
      // Battery
      const b = data.batt || {};
      html += `<div class="data-card">
        <h4>🔋 Batterie</h4>
        <div class="data-val" style="color: #38bdf8;">${b.battsoc || 0}%</div>
        <div class="data-val-sub">Spannung: ${b.VoltB || 0} V | Strom: ${b.Ampere || 0} A</div>
        <div class="data-val-sub">Starter: ${b.starter_voltage || 0} V</div>
      </div>`;

      // Solar
      const s = data.mppt || {};
      html += `<div class="data-card">
        <h4>☀️ Solar (MPPT)</h4>
        <div class="data-val" style="color: #e3b341;">${s.mppt_pv_watts || 0} W</div>
        <div class="data-val-sub">Strom: ${s.mppt_pv_amps || 0} A | Spannung: ${s.mppt_pv_volts || 0} V</div>
      </div>`;

      // Tanks
      const lvl = data.level || {};
      html += `<div class="data-card">
        <h4>💧 Tanks</h4>
        <div class="data-val-sub">Frischwasser: <strong>${(lvl.level1 && lvl.level1.state) || 0}%</strong></div>
        <div class="data-val-sub">Grauwasser: <strong>${(lvl.level2 && lvl.level2.state) || 0}%</strong></div>
      </div>`;

      // Temps
      const t = data.temp || {};
      html += `<div class="data-card">
        <h4>🌡️ Temperaturen</h4>
        <div class="data-val-sub">Innen: <strong>${(t.temp1 && t.temp1.state) || '--'} °C</strong></div>
        <div class="data-val-sub">Außen: <strong>${(t.temp2 && t.temp2.state) || '--'} °C</strong></div>
        <div class="data-val-sub">Feuchte: <strong>${(t.temp3 && t.temp3.state) || '--'} %</strong></div>
      </div>`;

      // Relays
      const r = data.relay || {};
      let rActive = 0;
      for (let k in r) { if (r[k].state) rActive++; }
      html += `<div class="data-card">
        <h4>🔌 Relais</h4>
        <div class="data-val" style="color: #3fb950;">${rActive} aktiv</div>
        <div class="data-val-sub">${Object.keys(r).length} Relais konfiguriert</div>
      </div>`;

      // Dimmers
      const d = data.dimmer || {};
      html += `<div class="data-card">
        <h4>💡 Dimmer</h4>
        <div class="data-val-sub">Dimmer 1: ${(d.dimmer1 && d.dimmer1.state) || 0}%</div>
        <div class="data-val-sub">Dimmer 2: ${(d.dimmer2 && d.dimmer2.state) || 0}%</div>
      </div>`;

      // Heater
      const h = (data.heater && data.heater.autoterm1) || {};
      html += `<div class="data-card">
        <h4>♨️ Standheizung</h4>
        <div class="data-val" style="font-size: 16px;">${h.heatstatus || 'Standby'}</div>
        <div class="data-val-sub">Modus: ${h.mode || 'temp'} | Ziel: ${h.targettemp_vanpi || 20}°C</div>
      </div>`;

      // MaxxFan
      const m = (data.maxxfan && data.maxxfan.maxxfan) || {};
      html += `<div class="data-card">
        <h4>🌀 MaxxFan</h4>
        <div class="data-val" style="font-size: 16px;">${m.fan_power ? 'EIN (Stufe ' + m.fan_speed + ')' : 'AUS'}</div>
        <div class="data-val-sub">Haube: ${m.fan_vent || 'close'} | Richtung: ${m.fan_direction || 'out'}</div>
      </div>`;

      container.innerHTML = html;
    });
}

// SSE Live Stream
function setupEventStream() {
  const ev = new EventSource('/api/events');
  ev.onmessage = (e) => {
    try {
      const log = JSON.parse(e.data);
      addLogRow(log);
    } catch(err) {}
  };
  ev.onerror = () => {
    setTimeout(setupEventStream, 3000);
  };
}

function addLogRow(log) {
  const tbody = document.getElementById('logTableBody');
  if (tbody.children.length === 1 && tbody.children[0].textContent.includes('Warte auf')) {
    tbody.innerHTML = '';
  }
  const tr = document.createElement('tr');
  const d = log.details || {};
  const isPut = log.message.startsWith('PUT');
  const methodClass = isPut ? 'method-put' : 'method-get';
  const statusColor = (d.status === 200 || !d.status) ? 'status-200' : 'status-err';
  const ms = d.ms ? (d.ms + 'ms') : '--';

  tr.innerHTML = `
    <td style="color: var(--text-muted);">${log.time}</td>
    <td style="color: #c9d1d9;">${d.client || 'System'}</td>
    <td class="${methodClass}">${log.message}</td>
    <td><span class="badge ${d.mode === 'LIVE_FULL' ? 'badge-green' : (d.mode === 'OFFLINE' ? 'badge-blue' : 'badge-yellow')}" style="padding: 2px 6px; font-size: 10px;">${d.mode || log.category}</span></td>
    <td class="${statusColor}">${ms}</td>
  `;
  tbody.insertBefore(tr, tbody.firstChild);
  if (tbody.children.length > 50) {
    tbody.removeChild(tbody.lastChild);
  }
}

// Init
updateStatus();
setupEventStream();
setInterval(updateStatus, 3000);
</script>

</body>
</html>
"""

# ---------------------------------------------------------------------------
# Server Startup & Main Execution
# ---------------------------------------------------------------------------

def run_gateway(port_display=1880, port_dashboard=8088):
    # Starte Fast-Cache Worker
    sync_thread = threading.Thread(target=background_sync_worker, daemon=True)
    sync_thread.start()

    # Erkenne IPs und Peers
    local_ips = get_local_ip_addresses()
    peers = get_tailscale_peers()
    
    # Auto-Erkennung von VanPi
    for p in peers:
        if p.get("is_likely_vanpi"):
            state.target_host = p["ip"]
            break

    primary_ip = local_ips[0]["ip"] if local_ips else "127.0.0.1"

    print("=" * 66)
    print("\033[96m        🚐 CamperUI ↔ VanPi Tailscale Gateway (Aktiv)\033[0m")
    print("=" * 66)
    print(f"\033[92m[PC IP für Display]\033[0m  \033[1m{primary_ip}\033[0m (Auf dem Display eintragen!)")
    print(f"\033[92m[Display Proxy Port]\033[0m 1880 (CamperUI Standard)")
    print(f"\033[93m[VanPi Ziel]\033[0m         {state.target_host}:{state.target_port} (via Tailscale)")
    print(f"\033[93m[Betriebsmodus]\033[0m      {state.mode.upper()} (Safe Mode: Schaltbefehle geschützt)")
    print(f"\033[94m[Web Dashboard]\033[0m      http://localhost:{port_dashboard}/")
    print("=" * 66)
    print("Anleitung:")
    print(f" 1. Auf dem Display: Einstellungen -> Netzwerk -> VanPi-IP: {primary_ip}")
    print(f" 2. Web-Dashboard im Browser öffnen: http://localhost:{port_dashboard}/")
    print(" 3. Drücke Strg+C zum Beenden.")
    print("=" * 66)

    # Server 1: Port 1880 für das Display
    server_1880 = ThreadedHTTPServer(("0.0.0.0", port_display), VanPiProxyHandler)
    t1 = threading.Thread(target=server_1880.serve_forever, daemon=True)
    t1.start()

    # Server 2: Port 8088 für das Web Dashboard & API
    server_8088 = ThreadedHTTPServer(("0.0.0.0", port_dashboard), DashboardAPIHandler)
    t2 = threading.Thread(target=server_8088.serve_forever, daemon=True)
    t2.start()

    state.log_event("SYSTEM", f"Gateway gestartet auf Port {port_display} (Proxy) & Port {port_dashboard} (Dashboard)")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[GATEWAY] Beende Gateway...")
        server_1880.shutdown()
        server_8088.shutdown()
        print("[GATEWAY] Beendet.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CamperUI <-> VanPi Tailscale Gateway")
    parser.add_argument("--port", type=int, default=1880, help="Proxy Port für Display (Standard: 1880)")
    parser.add_argument("--dashboard-port", type=int, default=8088, help="Web Dashboard Port (Standard: 8088)")
    parser.add_argument("--target", type=str, default=None, help="VanPi Tailscale Ziel-IP (Standard: Auto-Erkennung)")
    parser.add_argument("--mode", choices=["live_full", "live_safe", "offline"], default=None, help="Betriebsmodus")
    args = parser.parse_args()

    if args.target:
        state.target_host = args.target
    if args.mode:
        state.mode = args.mode

    run_gateway(port_display=args.port, port_dashboard=args.dashboard_port)
