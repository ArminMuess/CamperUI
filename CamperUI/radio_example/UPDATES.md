# Pflege bei neuen CamperUI-Versionen

Basis dieses Pakets: V1.9.1 / `ef71e15ed8110f5f1a01956fd5a1d3b5677f0550`.
Der V1-Radio-Patch auf V1.9.0 wurde auf diesen Stand übertragen. Einziger Git-Konflikt
war die Versionskennung; die offizielle V1.9.1-Kennung blieb als Basis des eigenen
Namens erhalten. Danach kamen Verbindungsdialog, Speicherung und Quellensteuerung hinzu.

## Trennung der Dateien

- `ui_radio.*`: Radioseite und bedienbare Quellenbereiche.
- `volumio_client.*`: Netzwerk-Worker, Status, Befehle und Adressgeneration.
- `volumio_protocol.h`: REST-Pfade und quellenunabhängige Regeln.
- `radio_config.*`: Verbindung/Presets, Validierung und eigener NVS-Namensraum.
- `radio_settings.*`: eigener LVGL-Dialog und eigene Websettings-/Diagnoserouten.
- `radio_tab_migration.h`: Navigation um den Radio-Tab ergänzen.
- `radio_example/`: Anleitung, Prüfungen, Diagnosehilfe.

Bestehende Dateien haben wenige gezielte Integrationen: Workerstart,
Radio-ID 13 / Tab-Anzahl 14, Navigation einschließlich Aufbauen/Update/Reset,
Einstellungsübernahme, Webrouten-Registrierung und eigene Versionskennung.
Die bestehenden Tab-IDs und die originale `settings_migration.h` bleiben unverändert.
Radioeinstellungen liegen unabhängig von Pekaway-/CamperUI-Settings im NVS-Namensraum
`camper_radio` als versionierter Datensatz. Bei neuem Flashformat ist eine Migration nötig.

## Reproduzieren

Im Archiv liegt `radio-v2.patch` als kombinierter Git-Patch gegen den obigen Basiscommit:

```sh
git clone https://github.com/NiklasJester/CamperUI.git
cd CamperUI
git switch -c armin/radio-v2 ef71e15ed8110f5f1a01956fd5a1d3b5677f0550
git apply --check /PFAD/ZU/radio-v2.patch
git apply /PFAD/ZU/radio-v2.patch
git add .
git commit -m 'Add local CamperUI radio extension V2'
```

Dafür braucht Git eine eigene konfigurierte Benutzeridentität. Die Dateien sind
**lokal vorbereitet**, kein öffentlich veröffentlichter Fork oder Pull Request.

## Nächstes Upstream-Update

1. Bekannte Firmware und Einstellungen sichern, funktionierenden Git-Stand taggen.
2. Neue Entwicklung in einem eigenen Testbranch.
3. `git fetch origin`, dann gewünschtes Release/Commit mit `git merge --no-commit --no-ff` zusammenführen.
4. Konflikte verstehen und beide Änderungen erhalten. Besonders prüfen:
   - Ist die bisher freie ID 13 inzwischen durch Upstream belegt?
   - Haben sich Tab-Anzahl, Migrationsregeln, Theme-Reset oder Webserver geändert?
   - Haben neue Bibliotheken andere LVGL-/Arduino-APIs?
5. Native Prüfungen, dann vollständiger Firmware-Build und Gerätetest.
6. Erst nach Erfolg Commit und eigene Versionskennung erstellen.

Kein blindes Ersetzen ganzer Upstream-Dateien. Ein konfliktfreier Merge beweist
keine Funktion. Solange der Merge offen ist, kann `git merge --abort` ihn abbrechen.
Ein Original-Update als .bin entfernt unsere Radio-Erweiterung; auf dem ESP32 ist
für die Rückkehr eine vorher gesicherte Firmware separat zurückzuflashen.
Die Originalfirmware kann die längeren Radio-Tab-Arrays ignorieren/zurücksetzen;
Navigationseinstellungen nach Rückkehr überprüfen.
