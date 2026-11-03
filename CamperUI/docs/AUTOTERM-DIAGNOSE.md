# Autoterm-Diagnose

Im Autoterm-Heizungstab öffnet die **Stethoskop-Taste rechts neben Start/Stopp** ein modales Fenster. **Zurück** schließt es. Die Bedienung sendet keine Heizungsbefehle. Start/Stop, Betriebsart und Solltemperatur bleiben unverändert.

Die Werte kommen aus der vorhandenen VanPi-Abfrage `GET /heater`, Objekt `autoterm1` (erste Autoterm, unabhängig vom Modellnamen 2D). Es ist kein zusätzlicher Userflow nötig. Die Anzeige nutzt den bestehenden Hintergrund-HTTP-Worker; beim Öffnen wird weder blockierend abgefragt noch ein zusätzlicher Polling-Task gestartet.

| Anzeige | VanPi-Feld | Einheit |
| --- | --- | --- |
| Spannung | heatvolt | V |
| Lüfterdrehzahl | heatfan | U/min |
| Kraftstoffpumpe | heatpump, sonst heatglow | Hz |
| Heizgerätetemperatur | heattemp | °C |

Der mitgelieferte VanPi-Flow `get heater stats` benennt die Kraftstoffpumpenfrequenz historisch `heatglow`, befüllt sie aber aus `heatpump_autoterm1` (frequencyfuelpump/100). Das ist keine Glühkerzentemperatur. Die übrigen Werte sind bereits in V, rpm und °C umgerechnet und werden nicht erneut skaliert. Die Heizgerätetemperatur ist keine Raumtemperatur.

Fehlende, null- oder nichtnumerische Messwerte erscheinen als `--`. Numerische Zeichenketten und Dezimalkommas werden unterstützt; gemessene Nullen bleiben gültig. Neue Antworten löschen die Verfügbarkeitsmaske, damit fehlende Felder keine alten Werte behalten. Bei getrennter Verbindung oder nach mehr als 45 Sekunden ohne gültige Heizungsantwort werden die Messwerte ausgeblendet. Das Alter bezeichnet die letzte HTTP-Antwort, nicht einen geräteseitigen Messzeitstempel. Der VanPi kann intern ältere Gerätewerte liefern; dies ist ohne zusätzlichen Zeitstempel nicht erkennbar.

Status und Fehler werden angezeigt. Im Demo-Modus gibt es ausdrücklich gekennzeichnete Beispielwerte. Bei einem Wechsel zwischen Demo und Live werden vorherige Diagnosedaten verworfen. Das Fenster wird beim UI-Neuaufbau entfernt. Es gibt keine Fehler-Reset- oder sonstigen Service-Schaltbefehle.

Live-Abgleich mit der VanPi-Webseite und Touch-/Langzeittests am Display stehen aus.
