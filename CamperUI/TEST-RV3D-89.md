# V1.2.1-RV3D.89-dev – Korrekturen und Gerätetests

Basis: Master V1.2.1, Commit 56e230d. Ohne Änderungen an RGB-Timing oder Displaytreibern.

## MaxxFan: Auto verlassen

Ursache im mitgelieferten VanPi-Flow: PUT /maxxfan/auto aktiviert Auto, beendet es aber nicht. Deshalb wartete der bisherige Client beim Ausschalten vergeblich auf fan_auto=false.

Der Client verwendet zum Verlassen von Auto jetzt /power und prüft fan_power=false sowie fan_auto=false. Für Manuell schaltet ein anschließender Power-Befehl wieder ein. Für Aus wird eine noch offene Haube danach über /vent geschlossen. Bereits passende Zustände werden übersprungen. Bei fehlender Bestätigung werden Umschaltbefehle weiterhin nicht automatisch wiederholt.

Offlineprüfung: `node tools/test_maxxfan_flow.cjs` führt die originalen VanPi-Funktionen aus docs/flows.json aus, ohne HTTP oder Gerätebefehle. Geprüft: Auto ist enable-only, Power beendet Auto bei laufendem und stehendem Lüfter, anschließendes Power aktiviert Manuell, Vent schließt bei ausgeschaltetem Lüfter. Die Prüfung ersetzt keinen Test am echten MaxxFan.

Gerätetest:
1. Live: Aus → Auto (26 °C) → Aus. Lüfterstopp und Haubenschließen am Gerät kontrollieren; keine Fehlermeldung.
2. Auto → Manuell: erst Auto aus, dann Lüfter an. Stufe und Richtung testen.
3. Lüfter aus, Haube offen → Aus: Haube schließt. Wiederholt Aus drücken: kein Einschalten.
4. WLAN/VanPi-Ausfall während eines Befehls: keine wiederholten Toggles, Meldung zeigt betroffene Zwischenstufe.
5. Original-Fernbedienung und andere Visus bedienen; prüfen, ob VanPi deren Änderungen überhaupt erfasst.

VanPi liefert intern nachgeführte Zustände. Eine Bestätigung beweist nicht die tatsächliche Hardwarestellung.

## Tab-Einstellungen

Der gemeldete Arduino-Absturz ist ohne zugehörige ELF-Datei nicht exakt lokalisierbar. Der Log mit nur 8772 freien LVGL-Bytes macht eine abweichende/zu kleine LVGL-Konfiguration zu einer plausiblen Ursache.

Builds mit internem LVGL-Pool unter 512 KiB werden jetzt mit einer konkreten Konfigurationsmeldung abgelehnt. config/lv_conf.h muss wie in der README beschrieben nach Arduino/libraries/lv_conf.h kopiert werden. Bei knappem Laufzeitspeicher wird die Tab-Seite nicht aufgebaut. Öffnen/Schließen erfolgen nach dem Touch-Callback; Speichern ruft den LVGL-Timerhandler nicht mehr rekursiv auf. Ein UI-Neuaufbau räumt auch die Tab-Einstellungsseite auf.

Test: Tabs anpassen mehrfach öffnen, scrollen, zurück; Reihenfolge/Sichtbarkeit ändern, speichern, Neustart. Sowohl Arduino-Installation als auch spätere OTA-Installation prüfen.

## Einstellungen

- Speichern & Beenden baut die UI nach Ende des Touch-Callbacks neu auf. Relais-/Dimmer-Sichtbarkeit soll sofort gelten, ohne Neustart.
- Die Firmware-Update-Karte liegt dynamisch unter dem vollständigen WLAN-Status mit Abstand. IP, Hostname und Verbindungsdiagnose dürfen nicht verdeckt werden.

Test: jeweils Relais, Dimmer und WLAN-Relais aus-/einblenden, speichern, betreffende Seite prüfen. WLAN verbunden/getrennt und lange SSID testen; QR nach WLAN-/IP-Wechsel kontrollieren.

## OTA-Neustart

Die Hintergrundbeleuchtung bleibt beim Update an. Ein erfolgreicher Upload wird erst nach vollständig abgeschlossenem Update als erfolgreich gemeldet. Der Neustart erfolgt zeitversetzt nach der HTTP-Antwort; bis dahin bleibt die normale UI-Verarbeitung pausiert. Vor dem Neustart wird die konfigurierte Helligkeit wieder gesetzt.

Dies beseitigt den expliziten Backlight-Off-Pfad. Ob der gemeldete schwarze Bildschirm vollständig verschwindet, ist erst am Gerät überprüfbar. Die Hardware kann weiterhin einen separaten Display-Reset benötigen.

Test: Den 89er-Stand zunächst über Arduino aufspielen, danach ein OTA-Update mit laufender 89er-Firmware testen. Ein Update aus V1.2.1 wird noch von deren bisheriger OTA-Routine ausgeführt. Erfolgreicher Upload → automatischer Neustart → Bild ohne Stromtrennung. Abbruch/ungültige Datei → kein Neustart und Display weiterhin bedienbar.

## Temperaturverlauf

Beschriftete Y-Achse mit vier Temperaturwerten in 0,1-°C-Auflösung; Platz links an der Kurve reserviert. Die Skala folgt dem Verlauf. Auch bei konstanten Temperaturen bleibt die Kurve um den tatsächlichen Messwert zentriert.

Test: 1/6/12/24 Stunden, 2/3/4 Felder, negative/konstante Werte und keine Daten. Achsenwerte müssen zur Min-/Max-Skala und Kurvenlage passen.
## Live-Test: Touch, Batterie und gemeinsame Temperaturen

- Der GT911 wird nur bei frischen Messdaten ausgewertet. Fehlende neue Messdaten gelten nicht als Loslassen. Kurze Aussetzer werden 60 ms überbrückt; nach 250 ms ohne gültige Messung wird ein festhängender Touch freigegeben.
- Relais und WLAN-Relais akzeptieren pro Ausgang höchstens einen Befehl innerhalb von 600 ms. Home-Favoriten nutzen dieselbe Sperre; verschiedene Ausgänge bleiben unabhängig.
- Batterie-Farben verwenden überall dieselben gespeicherten Grenzen: Rot bis einschließlich Rot-Schwelle (Standard 20 %), Gelb darüber bis einschließlich Gelb-Schwelle (Standard 40 %), Blau darüber. Rot muss kleiner als Gelb sein. Einstellungen befinden sich unter Alarm-Schwellen.
- Home übernimmt fest die Sensorzuordnung für Innen, Außen und Zusatz 1 aus den zentralen Temperatur-Einstellungen. Home konfiguriert nur deren Sichtbarkeit. Alte Home-Zuordnungen werden nicht mehr benutzt; ihre Sichtbarkeit wird beim ersten Laden übernommen. Ohne zugewiesenen Zusatz-1-Sensor bleibt das dritte Temperaturfeld ausgeblendet.

Automatische Prüfung: tools/test_input_debounce.cpp enthält sieben Compile-Time-Prüfungen der tatsächlichen Filterklassen (Halten, kurze Aussetzer, echtes Loslassen, Sensorausfall, unabhängige Relais, Zeitüberlauf, Reset). Mit einem C++17-Compiler und dem Projektordner als Include-Pfad kompilieren. Zusätzlich testet tools/test_maxxfan_flow.cjs die originalen Node-RED-Funktionen ohne Hardwarebefehle.

Am Display prüfen: Relais und Home-Favoriten jeweils mehrere Sekunden gedrückt halten und langsam loslassen; es darf nur einmal schalten. Schnell mehrfach drücken; keine Relaisfolge innerhalb der Sperrzeit. Andere Relais direkt danach separat bedienen. Scrollen und Display-Aufwecken testen. Batterie exakt an beiden Grenzen und direkt darüber prüfen, Grenzen ändern und neu starten. Zentrale Temperaturzuordnungen ändern: Home muss Innen/Außen/Zusatz 1 übernehmen, einschließlich Ruuvi-Sensoren. Home-Sichtbarkeit separat ein-/ausschalten und nach Neustart kontrollieren.


## Energie-Verläufe

Auf den Batterie- und Solar-Kacheln im Energie-Dashboard öffnet ein Chart-Symbol eine eigene große Verlauf-Ansicht. Zurück führt wieder zur Energie-Seite. Batterie bietet Ladezustand (%) und die aus VoltB × Ampere berechnete Leistung (W): positives Vorzeichen = Laden, negatives Vorzeichen = Entladen. Solar zeigt mppt_pv_watts (W), keinen Energieertrag in Wh. Zeitfenster: 1/6/12/24 Stunden; Y-Achse, Min/Max und relative Zeitachse sind beschriftet.

Die Historie zeichnet vorhandene gemeinsame Dummy- oder Live-Daten auf. Es werden keine Demo-Verläufe vorgefüllt. Pro Sekunde werden frische Werte gesammelt und pro Minute gemittelt. Bis zur ersten vollständigen gültigen Minute bleibt der Verlauf leer. Lange Zeitfenster fassen mehrere Minuten je Bildpunkt zusammen; Min/Max beziehen sich auf die gespeicherten Minutenmittel. Die letzten 24 Stunden benötigen etwa 9 KB PSRAM; kein Flash-Schreiben. Ohne PSRAM-Zuweisung zeigt die Ansicht einen Hinweis.

Die Aufnahme läuft auch bei geschlossener Ansicht und ausgeschaltetem Display weiter. Neustart und Dummy/Live-Wechsel leeren den Puffer; andere Seiten und UI-Neuaufbauten behalten ihn. Live-Werte müssen zum aktuellen Modus passen, per HTTP vorhanden sein und innerhalb der letzten 15 Sekunden aktualisiert worden sein. Ungültige JSON-Werte gelten nicht als frische Nullwerte. Ausfälle/unterbrochene Messminuten bilden Lücken; sie werden nicht mit alten Messwerten aufgefüllt.

Automatisch geprüft: tools/test_energy_history.cpp prüft den tatsächlichen Puffer für leeren Start, erste Messminute in langen Fenstern, fehlende Messungen, vorzeichenbehaftete Leistung, Mittelung, vollständige Fensterabdeckung, Umlauf des 24-Stunden-Puffers und Löschen alter Daten. Wie die Eingabefilter mit C++17 und dem Projektordner als Include-Pfad kompilieren.

Display-Test: Beide Chart-Symbole öffnen/schließen, jeweils mehrfach; Batterie-Messgröße und alle Zeitfenster wechseln. Zwei oder mehr Minuten Dummy-Modus laufen lassen: Werte/Einheiten und Vorzeichen müssen zu den Energie-Kacheln passen. In Live wechseln: alte Demo-Kurven sofort verschwunden, erst neue Live-Minuten sichtbar. WLAN/VanPi kurz und länger unterbrechen: Lücken, keine Null-Kurve und keine fortgeschriebenen Altwerte. Neustart: leerer Verlauf. Mindestens 24 Stunden laufen lassen: Puffer begrenzt und ältere Werte verschwinden. Dunkles/helles Design sowie Anzeige-Aufwecken prüfen. Mehrfach Einstellungen speichern und danach Ansichten wieder öffnen: keine alten UI-Zeiger und kein anhaltender Speicherverlust.

## Einheitliches Chart-Design

Temperatur-, Batterie- und Solar-Verläufe verwenden ui_chart_style.h für Tasten, Chart-Karten und Raster. Gleiche 1-px-Tastenrahmen, Rundungen, aktive/inaktive Farben und Schriftgrößen; gleiche Kartenrahmen und dezente Rasterlinien. Die Energie-Ansichten zeigen Min/Max im Chart-Kopf und Start/Mitte/Jetzt in der Zeitachse. Die größere eigene Ansicht behält große Touch-Tasten; bei Batterie-Leistung bleibt die Nulllinie mittig.

Sichtprüfung am Display: Beide Energie-Ansichten mit Temperaturen vergleichen, im dunklen und hellen Design. Inaktive/aktive Tastenrahmen, Text-Kontrast, Chart-Ränder und Min-/Max- sowie Zeitbeschriftungen auf Überlappungen prüfen.
