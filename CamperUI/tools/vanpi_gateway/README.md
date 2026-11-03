# 🚐 CamperUI ↔ VanPi Tailscale Gateway

> **Live-Testen des CamperUI-Displays am Schreibtisch – über Tailscale mit dem Camper verbunden!**

---

## 🎯 Das Problem & Die Lösung

Wenn sich dein Camper nicht in Reichweite des heimischen WLANs befindet, ist das VanPi-System meist über **Tailscale** (z. B. `pekaway` mit IP `100.80.161.23`) angebunden.

* **Das Problem:** Der ESP32-Mikrocontroller des Displays kann keinen vollwertigen Tailscale-Client ausführen. Wenn das Display im Heim-WLAN oder an einem PC-Hotspot angemeldet ist, kann es die Tailscale-IP (`100.x.y.z`) nicht direkt auflösen oder anrouten.
* **Die Lösung:** Dieses **Tailscale Gateway Tool** läuft auf deinem PC (der bereits mit Tailscale verbunden ist). Das Gateway lauscht auf Port `1880` und leitet alle Anfragen des Displays transparent an das VanPi-System im Fahrzeug weiter.

```
+-----------------------------------------------------------+
| 📱 CamperUI Display (ESP32-S3)                             |
| Einstellungen -> Netzwerk -> VanPi-IP: 192.168.1.170      |
+-----------------------------------------------------------+
                             │
                             │ Lokales WLAN (Heimnetz oder PC-Hotspot)
                             ▼
+-----------------------------------------------------------+
| 💻 PC (Windows) - VanPi Tailscale Gateway                 |
| Lauscht auf: 0.0.0.0:1880 (Proxy) & 0.0.0.0:8088 (Web UI) |
|                                                           |
| 🛡️ 3 Betriebsmodi:                                        |
|   1. SAFE MODE (Standard): Live-Lesen, Befehle simuliert  |
|   2. LIVE MODE: Schaltet echte Relais & Heizung im Camper |
|   3. OFFLINE MODE: Nutzt Snapshot (auch wenn VanPi aus)   |
|                                                           |
| ⚡ Fast-Response Cache: Schützt vor 800ms ESP32-Timeouts   |
+-----------------------------------------------------------+
                             │
                             │ Tailscale WireGuard Tunnel
                             ▼
+-----------------------------------------------------------+
| 🚐 Camper: VanPi System (pekaway: 100.80.161.23:1880)     |
| Node-RED REST API                                         |
+-----------------------------------------------------------+
```

---

## 🚀 Schnellstart in 3 Schritten

### 1. Gateway auf dem PC starten
Starte einfach per Doppelklick die Datei:
```cmd
start_gateway.bat
```
*(Alternativ im Terminal: `python tools\vanpi_gateway.py`)*

Das Tool startet das Gateway und öffnet automatisch das Web-Dashboard unter:
👉 **`http://localhost:8088/`**

### 2. PC-IP ablesen
Im Web-Dashboard und im Konsolenfenster wird deine lokale PC-IP hervorgehoben angezeigt, z. B.:
> **`192.168.1.170`** *(WLAN / Heimnetzwerk)*

### 3. Auf dem CamperUI-Display eintragen
1. Schalte das Display ein und verbinde es über WLAN mit demselben Netzwerk wie deinen PC (oder erstelle am PC einen mobilen Windows-Hotspot).
2. Tippe auf **Einstellungen** (Zahnrad) → **Netzwerk**.
3. Gib als **VanPi-IP** die oben abgelesene IP deines PCs ein (z. B. `192.168.1.170`).
4. **Fertig!** Der Port `1880` ist im Display fest hinterlegt. Das Display pollt ab sofort live deine Camper-Daten!

---

## 🛡️ Die 3 Betriebsmodi (Schutzschalter)

Im Web-Dashboard kannst du jederzeit zwischen 3 Modi umschalten:

| Modus | Beschreibung | Empfohlen für |
| :--- | :--- | :--- |
| 🛡️ **SAFE-Modus** *(Standard)* | Liest alle Live-Sensordaten (Batterie, Solar, Temperaturen, Wasser) echt aus dem Camper. **Schaltbefehle (PUT) werden abgefangen und nur lokal simuliert.** Die UI schaltet optisch um, aber es werden keine echten 12V-Relais oder die Standheizung im Camper aktiviert. | Schreibtisch-Tests & UI-Entwicklung ohne Risiko für das Fahrzeug. |
| ⚡ **LIVE-Modus (Voll)** | Echte 1:1 Weiterleitung aller GET- und PUT-Befehle an den Camper über Tailscale. Jeder Tastendruck auf dem Display schaltet echte Verbraucher im Fahrzeug. | Reale Funktionsprüfungen und Fernsteuerung. |
| 💾 **OFFLINE-Modus** | Verwendet einen lokal gespeicherten Snapshot (`camper_snapshot.json`). Funktioniert komplett ohne Verbindung zum Fahrzeug. | Tests, wenn der Camper stromlos ist oder kein Internetempfang besteht. |

---

## ⚡ Fast-Response Cache & Timeout-Schutz

* Das CamperUI-Display pollt alle 500 ms sequentiell einen Endpunkt (`/batt`, `/relay`, `/dimmer`, `/level`, `/temp`, `/heater`, `/mppt/`, etc.).
* Die ESP32-Firmware besitzt ein hartes HTTP-Timeout von **800 ms**. Falls eine Anfrage über Mobilfunk/Tailscale länger dauert, pausiert das Display für 5 Sekunden.
* **Die Lösung des Gateways:** Das Gateway aktualisiert die Daten im Hintergrund über Tailscale und beantwortet Anfragen des Displays in **1 bis 2 Millisekunden** aus dem Speicher. Das garantiert butterweiche 60 FPS und verhindert jegliches Ruckeln oder Verbindungsverluste!

---

## 📸 Snapshot-Funktion

* Im Web-Dashboard kannst du mit einem Klick auf **"📸 Aktuellen Camper-Zustand als Snapshot sichern"** die exakte Konfiguration deines Fahrzeugs abspeichern (alle Relaisnamen, Dimmer-Werte, Tank-Namen und Sensoren).
* Diese Daten bleiben in `camper_snapshot.json` erhalten und stehen auch im Offline-Modus zur Verfügung.

---

## 🔧 Fehlerbehebung & Windows Firewall

### Display zeigt "WLAN getrennt" oder "Verbindung fehlgeschlagen"
1. **Gleiches Netzwerk prüfen:** Befinden sich PC und Display im selben Subnetz (z. B. beide an der FritzBox oder beide am Windows-Hotspot)?
2. **Windows Defender Firewall:** Windows blockiert standardmäßig eingehende Verbindungen auf Port 1880 von anderen Geräten im Netzwerk.
   * Führe einfach mit Administrator-Rechten aus:
     ```cmd
     setup_firewall_rule.bat
     ```
   * Dies schaltet Port 1880 (TCP) dauerhaft für das Display frei.
3. **Tailscale Status prüfen:** Prüfe am PC im Terminal mit `tailscale status`, ob der Camper (`pekaway`) erreichbar ist.
