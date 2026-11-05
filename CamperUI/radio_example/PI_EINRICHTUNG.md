# Radio-Pi einrichten: Volumio 4 + DAB+-Stick + Alps-Drehgeber

Gilt für: Raspberry Pi 4, DACBerry, Nooelec NESDR SMArt v5, Alps-Drehgeber (EC11-Bauart, mit Taster).
Auf dem Pi wird **nichts programmiert** – alles läuft über zwei Volumio-Plugins.

Festgelegte Bedienung:

| Drehgeber | Wirkung |
|---|---|
| Drehen | Lautstärke |
| Kurz drücken | letzten Sender starten / Wiedergabe stoppen (Webradio **und** DAB+) |
| Display | bleibt dunkel, Senderwahl per Touch |

---

## 1. Drehgeber anschließen

Die DACBerry AMP² belegt laut OSA GPIO 2–3 (Konfiguration), 14–15 (MIDI), 18–21 (Audio)
und **GPIO 22 (Pin 15) für die Stummschaltung** – GPIO 22 darf also nicht für den Drehgeber verwendet werden
([GPIO-Übersicht der DACBerry-Boards](https://www.osaelectronics.com/learn/tutorials/gpio-usage-of-dacberry-series-boards/)).
Der Drehgeber liegt deshalb auf GPIO 23–25. Die fünf Pins sitzen nebeneinander in der Reihe der geraden Pins,
ein einziger 1×5-Stecker reicht.

Angeschlossen wird an der seitlichen Stiftleiste des GPIO-Verteilers (Geekworm G341), bevor die DACBerry
aufgesteckt und verschraubt wird.

| Pin | Raspberry Pi | Drehgeber-Anschluss |
|---|---|---|
| 14 | GND | C (Mitte, gemeinsamer Kontakt) |
| 16 | GPIO 23 | A (außen) |
| 18 | GPIO 24 | B (außen) |
| 20 | GND | Taster, Kontakt 2 |
| 22 | GPIO 25 | Taster, Kontakt 1 |

- Kein 3,3 V nötig: Die internen Pull-up-Widerstände des Pi reichen. Nur bei langen Leitungen (> 50 cm) 10 kΩ von A und B nach 3,3 V ergänzen.
- Dreht die Lautstärke falsch herum: A und B tauschen (oder im Plugin die Pins tauschen).

## 2. Volumio-Version prüfen

Einstellungen → System: **Volumio 4.x** ist Pflicht (beide Plugins laufen nur unter Volumio 4 / Bookworm).

## 3. DAB+ mit dem Nooelec-Stick

1. Stick mit Antenne einstecken (möglichst direkt am Pi, nicht über einen Hub).
2. Plugins → Plugins suchen → **FM/DAB Radio** (`rtlsdr_radio`) installieren und aktivieren. Danach einmal neu starten.
3. Im Plugin einen **DAB-Suchlauf** starten (dauert rund 5 Minuten).
4. Sender verwalten, umbenennen und Favoriten setzen: `http://<RADIO-IP>:3456/manage`
5. Fehler „usb_claim_interface error -6“: Stick ab- und wieder anstecken oder Pi neu starten.

### DAB-Sender aufs Display bringen

1. Gewünschten DAB-Sender in Volumio starten.
2. Im Browser `http://<RADIO-IP>/api/v1/getState` öffnen und den Wert bei **`uri`** kopieren
   (Form: `rtlsdr://dab/<Kanal>/<Sendername>`).
3. Am Display im Radio-Tab auf das Zahnrad (oder `http://<DISPLAY-IP>/radio`) und die Adresse beim passenden DAB-Preset eintragen.

## 4. Drehgeber in Volumio

1. Plugins → Plugins suchen → **Rotary Encoder II** installieren und aktivieren.
2. Einstellungen für **Encoder 1**:

| Feld (deutsche Oberfläche) | Wert |
|---|---|
| Encoder aktivieren | ein |
| Perioden pro Schritt | zuerst **1/1** (Volle Perioden) |
| Anschluss A GPIO | 23 |
| Anschluss B GPIO | 24 |
| Drehfunktion | **Lautstärke** |
| Taster GPIO | 25 |
| Entprell-Zeit (ms) | 50 |
| Taster-Logikpegel low-aktiv | ein |
| Funktion Tastendruck | **Pause an/aus** |
| Funktion langer / doppelter Tastendruck | `...` (nichts) oder Stumm |

3. Speichern. Zählt nur jede zweite Rastung, „Perioden pro Schritt“ auf **1/2** stellen; springt die Lautstärke doppelt, zurück auf 1/1.

Warum „Pause an/aus“ passt: Das Plugin schickt bei Webradio „Stopp“ und bei DAB „Pause“, und das DAB-Plugin behandelt Pause als Stopp. Der nächste Druck startet in beiden Fällen den zuletzt gespielten Sender aus der Warteschlange.

## 5. Volumio-Wiedergabeoptionen

- **Persistente Warteschlange: ein**, damit der Taster auch nach einem Neustart den letzten Sender startet.
- Lautstärkeschritte nach Geschmack (z. B. 2 oder 3 pro Rastung).
- Die Lautstärke regelt der Mixer der DACBerry; ob der Pegel passt, beim ersten Test mit niedriger Lautstärke prüfen.

## 6. Radio-Pi Ein/Aus mit Shelly 1 Gen3 (optional)

Das Display schaltet den Radio-Pi über ein Shelly-WLAN-Relais in dessen 12-V-Zuleitung –
mit sauberem Herunterfahren vorher. Keine Leitung zum VanPi nötig.

**Verkabelung** (alles hinter der Blende):

| Shelly-Klemme | Anschluss |
|---|---|
| 12 V **+** | 12 V+ (abgesichert) |
| 12 V **⊥** (Masse) | Masse |
| **I** (Relais-Eingang) | 12 V+ (abgesichert) |
| **O** (Relais-Ausgang) | 12-V-Eingang der DACBerry (versorgt auch den Pi) |
| **SW** | frei lassen |

**Shelly einrichten** (über seine eigene Weboberfläche, nicht nötig: Shelly-App/Cloud):
1. Mit dem WLAN des Shelly verbinden, `192.168.33.1` öffnen, euer Camper-WLAN eintragen.
2. Im Router (RUTX50) dem Shelly eine **feste IP** geben (DHCP-Reservierung).
3. Ausgang: **Einschaltverhalten nach Stromausfall = letzter Zustand** („Restore last“).
4. Eingang: **getrennt / detached**, damit am SW-Eingang nichts den Strom schalten kann.
5. **Keinen Passwortschutz** für die lokale Weboberfläche aktivieren – das Display meldet sich nicht an.
6. Firmware aktualisieren.

**Am Display:** Zahnrad im Radio-Tab (oder `http://<DISPLAY-IP>/radio`) → **Shelly-IP** eintragen,
optional **Akku-Schutz** in % (z. B. 25). Danach erscheint der Ein/Aus-Knopf neben dem Zahnrad:
- **Tippen** = Radio-Pi einschalten (grün = an, orange = startet / fährt herunter).
- **Lange drücken** = herunterfahren und danach Strom aus.

Das Shelly **nicht** zusätzlich als VanPi-WLAN-Relais anmelden – sonst gäbe es dort einen Schalter,
der den Strom ohne Herunterfahren trennt.

## 7. Abnahmetest

1. Webradio-Preset am Display antippen → Ton, Titel erscheint.
2. Drehgeber drehen → Lautstärke ändert sich, Display zeigt den neuen Wert (nach spätestens einer Abfrage).
3. Kurz drücken → Stopp; nochmal drücken → derselbe Sender läuft wieder.
4. DAB-Preset antippen → Ton; Drücken stoppt, erneutes Drücken startet ihn wieder.
5. Pi neu starten → Drücken startet den letzten Sender.
6. Mit Shelly: Ein/Aus-Knopf lange drücken → Volumio fährt herunter, nach etwa 30–40 s klickt das Relais.
   Danach tippen → Pi startet, nach der Bootzeit zeigt der Radio-Tab wieder Sender und Titel.
