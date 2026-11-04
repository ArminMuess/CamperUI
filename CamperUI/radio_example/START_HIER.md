# CamperUI Radio V2 – Entwicklungsstand

**Noch nicht flashen: Quellcode, kein vollständig kompilierter ESP32-Build.**
Basis: CamperUI **V1.9.1**, exakt Commit
`ef71e15ed8110f5f1a01956fd5a1d3b5677f0550`.
Radio-Firmwarekennung: `V1.9.1-radio-dev.3`. Datum: 04.11.2026.

## Neu in radio-dev.3

- **Stromsparende Abfrage:** Volumio wird nur jede Sekunde gefragt, solange der Radio-Tab
  sichtbar ist; bei anderem Tab alle 15 s, bei dunklem Display / Bildschirmschoner gar nicht.
  Beim Öffnen des Tabs wird sofort neu abgefragt.
- **Sonderzeichen:** Gedankenstriche, typografische Anführungszeichen, „…“ und Akzentbuchstaben
  (é, ñ, ç …) aus Sendermetadaten werden in darstellbare Zeichen umgewandelt (keine Kästchen mehr).
  Umlaute und € bleiben erhalten; Displaytexte schreiben jetzt „Lautstärke“, „Zurück“ usw.
- **DAB-Knöpfe** zeigen den Sendernamen aus der eingetragenen DAB-Adresse; derselbe Name geht
  als Titel an Volumio. Leere Presets heißen „DAB 1–3“.
- **Fehlermeldungen von Volumio** (HTTP 200 mit `error`) werden als Fehler angezeigt.
- **Laufender Sender grün markiert:** DAB über die Adresse; Internet-Presets nach dem ersten Start
  vom Display (die Stream-Adresse wird dabei gelernt, gilt bis zum Neustart des Displays).

## Was vorbereitet ist

- Radio-Tab mit Internet / DAB+ / Spotify-Bedienbereichen.
- Drei Internet-Presets: hr3, RADIO BOB!, ROCK ANTENNE.
- Start/Stopp, Lautstärke und echte Metadaten aus Volumio getState.
- Spotify: Zurück/Pause/Weiter und Start/Stopp, **wenn Volumio Spotify als aktive
  Quelle meldet**. Musik zuerst mit Spotify am Handy oder in Volumio auswählen.
  Kein eigener Spotify-Login, kein Katalog oder Playlistbrowser auf dem Display.
- DAB+: drei optional konfigurierbare Sender-URIs des FM/DAB-Radio-Plugins.
  Leere Presets bleiben deaktiviert. Kein Scan vom Panel und keine automatische
  Umschaltung bei Internetausfall.
- Zahnrad im Radio-Tab: Verbindung aktivieren, IP/Hostname, Port, Playlistnamen
  und DAB-Adressen eingeben. Alternativ im Browser `http://DISPLAY-IP/radio`.
- Einstellungen bleiben auf dem ESP32 gespeichert. Eine neue Radio-Pi-IP erfordert
  kein erneutes Flashen. Beide Einstellungswege lesen/schreiben dieselben Werte.
- Separater Netzwerk-Worker; LVGL-Callbacks führen keine HTTP-Anfragen aus.
- Alte Befehle verfallen nach 2,5 Sekunden und werden nicht wiederholt.
  Nach Adresswechsel verwirft der Worker alte Antworten und noch nicht gesendete
  Befehle. Eine bereits gesendete Anfrage lässt sich nicht zurückholen.
- CamperUI-Demomodus unterbindet die Radio-Anfragen. Für den echten Radiotest
  Demo ausschalten; ein fehlender VanPi wird dadurch nicht automatisch ersetzt.
- Quellenwahl wechselt **nur die sichtbaren Bedienelemente**, nicht die laufende Musik.
  Internet/DAB starten erst beim Antippen eines Presets. Spotify wird extern gewählt.

## Was wir später noch vor Ort prüfen

1. Alle CamperUI-Quellen und Theme-/Navigationsfunktionen bleiben korrekt.
2. Volumio 4.231, echte Plugin-Versionen, WLAN-Wiederverbindung und Reaktionszeit.
3. Echte Spotify-Befehle, DAB-Senderwahl, aktualisierte Metadaten und Audio.
4. Lautstärkemixer der DACBerry, später Drehgeber und Ruheverbrauch.
5. Langzeitbetrieb, Speicherbedarf, Watchdog und Verhalten bei Firmwareupdates.

Noch offen: vollständige ESP32-Kompilierung, RAM/Flashgröße, Displaytest.
Das Arduino-CLI-Werkzeug ließ sich hier laden; sein Paketdownload scheiterte
am fehlenden direkten Netzwerkzugriff. Es wird deshalb **keine .bin** ausgeliefert.
Die nativen Tests sind keine Firmware- oder Hardwarefreigabe.

## Einrichtung am Ersatzdisplay (erst nach erfolgreichem Build)

1. Funktionierende Originalfirmware und CamperUI-Einstellungen sichern.
2. Eigene Firmware kompilieren und als Testversion flashen.
3. Im Radio-Tab Zahnrad öffnen oder `/radio` im Browser aufrufen.
4. Volumio-Adresse ohne `http://` eingeben, Standardport 80; speichern.
5. In Volumio drei Playlists mit je einem Sender anlegen:
   `Camper hr3`, `Camper Radio BOB`, `Camper Rockantenne`.
   Namen können auf dem Display/im Browser angepasst werden.
6. Lautstärke vor dem Tontest niedrig stellen. Für DACBerry-Ton ist deren passende
   Versorgung nötig; USB-C-Versorgung des Pi vorher entfernen.

### DAB später ergänzen

Den DAB-Sender zunächst in Volumio starten. Unter
`http://RADIO-IP/api/v1/getState` bzw. `/api/v1/getQueue` die exakte Sender-URI
ermitteln. Format beim untersuchten Plugin:
`rtlsdr://dab/KANAL/URL-KODIERTER-SENDERNAME`.
Beispiel **nur für das Format**, kein garantierter empfangbarer Sender:
`rtlsdr://dab/5C/RADIO%20BOB%21`.
Diese vollständige URI beim passenden DAB-Preset eintragen. Ein Sendersuchlauf
und die Empfangsprüfung bleiben in Volumio. Nicht alle drei Wunschsender müssen
am Standort über DAB verfügbar sein.

### Diagnose ohne Veränderungen an Volumio

`radio_example/probe_volumio.py` fragt Status, Warteschlange, Playlists und
DAB-Browse-Liste ab. Es startet keine Wiedergabe, keinen Suchlauf und verändert
keine Einstellung:

```sh
python3 radio_example/probe_volumio.py RADIO-IP
```

Alternativ ohne Python zunächst einfach den getState-Link im Browser öffnen.
Der ESP32 liefert unter `/api/radio/status` seinen zuletzt beobachteten Zustand.
Veraltete Metadaten dort sind Diagnosewerte und nur mit `online: true` aktuell.

## Verifikation hier

`sh radio_example/run_tests.sh` führt aus:

- Native C++-Tests für Befehle, UTF-8-Encoding, Wertebereiche, Millis-Überlauf.
- Acht Kombinationen der Übernahme alter CamperUI-Navigationseinstellungen.
- Host-/DAB-URI-Prüfungen, Quellenkennung und Adressgeneration/Ablaufzeiten.
- Tatsächliche `radio_config.cpp` mit simuliertem NVS: speichern, laden,
  fehlgeschlagener Schreibversuch, korrupter Datensatz, Generationswechsel.
- Python-Diagnose mit simulierten HTTP-Antworten: nur lesende URLs, Fehlerfälle.
- Syntaxprüfung des neuen Websettings-JavaScripts und `git diff --check`.

**Bestanden. Nicht getestet:** Firmware-Gesamtbuild, FreeRTOS-Parallelität,
LVGL-Darstellung und echte HTTP-/Plugin-/Audiokommunikation.

## Build auf einem Rechner mit Arduino-Umgebung

Die vollständigen Upstream-Abhängigkeiten müssen installiert sein. Ausgangspunkt
laut Upstream-README: Arduino-ESP32 3.3.12, LVGL 8.4.0, Arduino_GFX 1.5.3,
zusätzlich SensorLib und ArduinoJson 7. Vorher `config/lv_conf.h` passend installieren.
Der Sketchordner im Paket heißt bereits `CamperUI`.

```sh
arduino-cli compile --output-dir build --fqbn esp32:esp32:esp32s3:CPUFreq=240,CDCOnBoot=cdc,PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB CamperUI.ino
```

Diese Versionskombination stammt aus Upstream, sie wurde hier nicht als
Gesamtbuild bestätigt. Keine Original-.bin aus dem Projekt mit unserer Erweiterung
verwechseln: Originaldateien enthalten keinen Radio-Tab.
