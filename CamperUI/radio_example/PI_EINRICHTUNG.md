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

Die Pins sind so gewählt, dass sie nicht mit der DACBerry kollidieren
(die DACBerry nutzt üblicherweise I²S an GPIO 18–21 und I²C an GPIO 2/3 – bitte mit der Anleitung deines Modells gegenprüfen).

| Drehgeber-Anschluss | Raspberry Pi | Physischer Pin |
|---|---|---|
| A (außen) | GPIO 17 | 11 |
| C (Mitte, gemeinsamer Kontakt) | GND | 14 |
| B (außen) | GPIO 27 | 13 |
| Taster, Kontakt 1 | GPIO 22 | 15 |
| Taster, Kontakt 2 | GND | 20 |

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
| Anschluss A GPIO | 17 |
| Anschluss B GPIO | 27 |
| Drehfunktion | **Lautstärke** |
| Taster GPIO | 22 |
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

## 6. Abnahmetest

1. Webradio-Preset am Display antippen → Ton, Titel erscheint.
2. Drehgeber drehen → Lautstärke ändert sich, Display zeigt den neuen Wert (nach spätestens einer Abfrage).
3. Kurz drücken → Stopp; nochmal drücken → derselbe Sender läuft wieder.
4. DAB-Preset antippen → Ton; Drücken stoppt, erneutes Drücken startet ihn wieder.
5. Pi neu starten → Drücken startet den letzten Sender.
