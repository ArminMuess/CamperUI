# Navigation v8 auf dem Display pruefen

Dieser Stand hat neun Hauptseiten: Home, Licht, Energie, Wasser, Klima,
MaxxFan, Schalter, Wasserwaage und Einstellungen. Die untere Leiste wird
nur mit dem Finger horizontal verschoben. Antippen aendert die Seite,
ohne die Leiste automatisch zu verschieben. Die Hauptseiten selbst haben
keine horizontale Wischnavigation.

## Vorbereitung

- Den vollstaendigen aktuellen Sketch kompilieren und auf das Display laden.
- Seriellen Monitor mit 115200 Baud oeffnen. Die Startausgabe muss
  `[NAV <Firmware-Version>] init` enthalten; so ist die geflashte Version erkennbar.
- Erwartete Werte: `faults=0`, `nav=(0,420) 480x60`,
  `scroll=(...,0)`, `content=(0,40) 480x380`.
- Fuer den unbeaufsichtigten Test den Display-Timeout ausreichend hoch setzen.

## Zuerst ohne Touch

1. Home mindestens drei Minuten geoeffnet lassen, ohne das Display zu beruehren.
2. Die Leiste muss durchgehend unten bleiben und darf den Inhalt nicht ueberlagern.
3. Falls sie springt, im seriellen Monitor `n` senden. Das gibt die gemessene
   Geometrie, freien LVGL-Speicher und einen LVGL-Pool-Integritaetstest aus.
4. Automatische `[NAV <Firmware-Version>] before-data`/`after-data`-Meldungen erscheinen nur
   bei einer erkannten Abweichung oder der Rueckkehr zum Normalzustand.
   Die Kontrolle liest Koordinaten und setzt die Leiste nicht zurueck.

## Anschliessend Bedienung

1. Leiste in beide Richtungen verschieben; nach dem Loslassen muss sie stoppen.
2. Jede der neun Seiten mehrfach auswaehlen, auch teilweise sichtbare Randbuttons.
   Kein selbststaendiges Nachscrollen, keine vertikale Bewegung, genau eine
   aktive Markierung. Ein Ziehen darf keine andere Seite auswaehlen.
3. Home und MaxxFan vertikal scrollen. Die untere Leiste bleibt bei y=420.
4. System-Einstellungen oeffnen und zurueckkehren.
5. Farbmodus mehrmals wechseln. Aktive Hauptseite und horizontale
   Leistenposition bleiben erhalten; Symbole behalten ihre Groesse.
6. MaxxFan AUF/ZU sowie EIN/AUS pruefen. Home-Favoriten und MaxxFan bleiben Demos.

## Wenn die Leiste trotzdem falsch aussieht

- `faults` ungleich 0 oder y ungleich 420: tatsaechliche Geometrieabweichung.
  Ausgabe vor/nach dem Springen sichern.
- Leiste optisch falsch, aber `faults=0` und `nav=(0,420) 480x60`:
  Der LVGL-Container liegt richtig. Dann Anzeige-/Renderpfad untersuchen;
  allein aus diesem Ergebnis folgt noch kein bestimmter Treiberfehler.
- `LVGL pool integrity: FAILED`: Speicherverwaltung des LVGL-Pools ist beschaedigt.
  `OK` schliesst andere Speicherfehler oder Nebenlaeufigkeitsfehler nicht aus.

Erst nach bestandenem Ruhe- und Bedienungstest ist die Hardwarestabilitaet
bestaetigt. Ein erfolgreicher Build allein bestaetigt sie nicht.


# Historischer Testbericht: CamperUI V8.7

## Aenderungen

- Home: Batterie- und Solar-Karte nebeneinander gleich breit (228 Pixel; 8 Pixel Abstand).
- Solar-Leistung verwendet dieselbe orange Farbe wie Batterie-Leistung.
- Batterie-Ueberschrift: ueber 40 % gruen, 20 bis einschliesslich 40 % gelb, unter 20 % rot; ohne empfangenen SOC neutral.
- Alle vier Home-Checkboxen haben feste, groessere Touchflaechen (212 × 40 Pixel) mit Abstand. Die LVGL-Checkbox selbst schaltet; es gibt keinen zweiten umschaltenden Klickhandler.
- WLAN-Passwort: Auge zeigt/versteckt den eingegebenen Text. Beim erneuten Oeffnen der System-Einstellungen ist das Passwort wieder verdeckt.
- Netzwerkseite: tatsaechlicher WLAN-Status, Netzwerkname, IP, Signalstaerke, Hostname, letzte Trennungsursache und Hinweise. Dummy-Daten simulieren keinen WLAN-Verbindungsstatus mehr.
- WLAN und VanPi sind getrennt erkennbar. Eine funktionierende WLAN-Verbindung bedeutet noch nicht, dass VanPi erreichbar ist.
- WLAN-Fehlercodes werden als Hinweise dargestellt. Ein Anmeldefehler beweist kein falsches Passwort: auch Signal oder Router-Regeln kommen infrage.

## Pruefung auf dem Display

1. Solar/Batterie nebeneinander vergleichen. Im Dummy-Modus muss Batterie 88 % gruen sein und Solar-Leistung orange erscheinen.
2. Alle vier Checkboxen einzeln mehrfach am Kaestchen und am Text antippen; beim Scrollen darf keine versehentliche Umschaltung auftreten. Einstellungen nach Neustart pruefen.
3. Auge zweimal betaetigen, Passwort bearbeiten, Seite verlassen und erneut oeffnen: Text wieder verdeckt. Das Passwort wird nicht in Diagnosemeldungen ausgegeben.
4. WLAN-Verbindung und zugewiesene IP auf Netzwerkseite pruefen, auch wenn Dummy-Modus aktiv ist.
5. Bei nicht erreichbarem WLAN die Fehleranzeige pruefen. Nach erfolgreicher Wiederverbindung soll der alte Fehler verschwinden. Danach echten VanPi-Zugriff separat pruefen.

Das sporadische Schwarzbild beim Start ist mit diesem Update nicht als behoben nachgewiesen. Display-Treiber, RGB-Timing und Zeichenpuffer bleiben unveraendert; die WLAN-Diagnose kann bei der weiteren Eingrenzung helfen.
