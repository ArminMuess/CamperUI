# V1.0.3-RV3D.88-dev

Basis: NiklasJester/CamperUI master, Commit 1853848 (V1.0.3).

## Änderungen

- Home ohne Seitentitel und ohne lokalen Dummy-/Live-Schalter. Die gemeinsame Umschaltung bleibt in den Einstellungen.
- Version/Demo in der festen oberen Statusleiste, Favoriten 56 statt 36 Pixel hoch.
- Ganze Home-Kacheln führen zu Energie, Klima oder Wasser. Ausgeblendete Zielseiten werden nicht automatisch aktiviert.
- Gleich breite Energie-Kacheln und orange Solarleistung bleiben erhalten.
- MaxxFan mit Aus/Manuell/Auto: große Tasten, nur passende Bedienung je Modus, vorhandene Haubengrafiken.
- Gemeinsamer Dummy-/Live-Modus gilt auch für MaxxFan. GET /maxxfan/ und PUT /maxxfan/{power,auto,vent,direction,speed/:value,temp/:value}.
- Vor Live-Befehlen frischer Zustand, anschließend Rückmeldung; keine automatischen Wiederholungen von Umschaltbefehlen. Weitere Eingaben warten auf Bestätigung (maximal 10 Sekunden). Verarbeitung ausschließlich im bestehenden HTTP-Worker; LVGL bleibt auf dem UI-Core.
- Alte/ungültige MaxxFan-Daten und WLAN-Ausfall sperren die Bedienung. Der Referenzflow liefert jedoch Defaults auch ohne angeschlossenen Lüfter; HTTP-Erfolg beweist keine aktive Hardware.
- Batterie und Solar schwanken langsam über 4 beziehungsweise 3 Minuten. Batterieleistung ergibt sich aus Spannung und Strom; Solarstrom aus Leistung/Spannung. Dieselben Parser/Daten werden auf allen Seiten verwendet.
- Ruuvi-Temperaturen aus /temp unter ruuvitag0..8 können Home-Feldern zugeordnet werden. Sensor-IDs bleiben bei Namensänderungen, Neustart und Ausfall erhalten. Der Referenzflow liefert dort gewöhnlich ruuvitag0..2; weitere IDs nur, falls der aktive Flow sie bereitstellt.
- Firmware-Upload-QR unter System-Einstellungen → Netzwerk. Lokale Erzeugung, aktuelle WLAN-IP + /update, Neuerzeugung bei IP-Wechsel, verborgen ohne WLAN. Keine zusätzliche Bibliotheksinstallation erforderlich.
- QR-Generator aus LVGL 8.4 / Project Nayuki, MIT-Lizenz in den Quelldateien erhalten; lokale Include-Pfade und Assertions angepasst, Symbole mit Camper-Präfix gegen Konflikte bei aktiviertem LVGL-QR-Modul.

## Display-Test

1. Firmware starten: korrekte Navigation, keine Verschiebung. Danach mindestens 10 Minuten bedienen/stehen lassen; USB und Powerbank vergleichen.
2. Dummy in Einstellungen einschalten. Home zeigt große Favoriten, Version oben und keine Überschriften/Modustaste. Kacheln öffnen die jeweils aktivierte Zielseite; Wischen löst keine Navigation aus.
3. Energie/Home vergleichen: übereinstimmende Werte und langsam veränderliche Leistung, Strom, Spannung und SOC.
4. MaxxFan-Dummy: Aus → nur Haube; Manuell → Stufe/Richtung/Haube; Auto → Temperatur und Status. Grenzwerte Stufe 1..10, Solltemperatur -2..37. Zwischen Seiten wechseln; Zustand bleibt erhalten.
5. Ruuvi-Dummy: drei Tags in Temperaturauswahl zuordnen, speichern, Neustart. Live: tatsächliche /temp-Antwort vergleichen, Tag-Ausfall und Wiederkehr prüfen. Fehlende Daten zeigen --; keine fremde Zuordnung.
6. QR mit Handy im selben WLAN scannen: URL muss auf die aktuelle ESP-IP mit /update führen. WLAN trennen: QR verschwindet. IP ändern: Code und Text folgen. BIN-Datei am Handy auswählen und vorhandenen Upload-Ablauf prüfen.

## MaxxFan-Live-Test (echtes Gerät erforderlich)

1. Dummy ausschalten; VanPi-IP konfigurieren. /maxxfan/ muss alle sechs erwarteten Felder enthalten.
2. Alle drei Modi, Haube, Richtung, Stufe und Temperatur am echten Lüfter prüfen; Status mit VanPi vergleichen. Auto kann den Lüfter je nach Temperatur selbst stoppen.
3. Während eines Befehls mehrfach tippen: kein zusätzlicher Umschaltbefehl. Änderungen über eine andere Visu erscheinen auf dem Display.
4. WLAN/VanPi trennen sowie HTTP 403/404/500, ungültiges JSON und fehlende Felder prüfen: Bedienung gesperrt beziehungsweise verständliche Meldung. Keine Umschalt-Wiederholung nach Timeout.
5. Während einer ausstehenden Live-Rückmeldung auf Dummy wechseln: keine weiteren Live-Schaltbefehle; anschließend wieder auf Live wechseln und Zustand neu lesen.

Die Firmware ist erst nach diesen Geräte- und Langzeittests als stabil beurteilt. Ein erfolgreicher Build ersetzt diese Tests nicht.

## Ergänzungen für den aktuellen Teststand

- Batterie-Dummy: 5–95 % über vier Minuten; Grün, Gelb und Rot prüfen. Das zusätzliche Batterie-Warnbadge ist entfernt, die normale SOC-Anzeige bleibt.
- MaxxFan-Aus: Auto deaktivieren, Lüfter ausschalten, Haube schließen. Bereits passende Zustände lösen keinen weiteren Toggle aus; jede Zwischenstufe wird über VanPi geprüft.
- Im manuellen Modus wechseln die Haubenbilder entsprechend Offen/Zu.
- Live: Aus aus Manuell und Auto sowie bei bereits ausgeschaltetem Lüfter mit offener Haube testen. Am Gerät kontrollieren, nicht nur in VanPi.
- Original-Fernbedienung bedienen und prüfen, ob VanPi die Änderung mitbekommt. Der gespeicherte VanPi-Zustand ist keine bestätigte Hardware-Rückmeldung.
