# Schnittstellenprüfung für Spotify und DAB+

Quellen am 02.11.2026 gelesen, keine Verbindung zum Nutzer-Radio-Pi:

- https://developers.volumio.com/api/rest-api
- https://github.com/volumio/volumio-plugins-sources-bookworm/blob/master/spotify/index.js
- https://github.com/volumio/volumio-plugins-sources-bookworm/blob/master/rtlsdr_radio/index.js
- https://github.com/volumio/volumio-plugins-sources-bookworm/blob/master/rtlsdr_radio/README.md

Spotify veröffentlicht den Zustand mit Servicekennung `spop`; im Code gibt es
Pause/Resume/Stop und Spotify-Connect-/Volatile-Logik. Daher wird kein eigener
Login umgesetzt. Musik extern wählen; die vom Player veröffentlichten Metadaten
anzeigen und generische Volumio-Befehle senden. Unterstützung auf dem installierten
Plugin ist noch real zu prüfen. Quelle `spotify` wird zusätzlich als Kennung akzeptiert.

FM/DAB-Radio nutzt Service `rtlsdr_radio`. DAB-Browse-Elemente haben Typ `webradio`
und URI `rtlsdr://dab/<channel>/<encodeURIComponent(exactName)>`.
Das Plugin implementiert `explodeUri` und `clearAddPlayTrack`. Der vorbereitete
Preset-Aufruf nutzt den dokumentierten Volumio-Endpunkt POST `/api/v1/replaceAndPlay`
mit `item` (service/type/title/uri). Der Aufruf ersetzt die aktuelle Queue.
Ob der installierte Pluginstand/Volumio 4.231 exakt so zusammenarbeitet, bleibt
bis zum Hardware-/Netzwerktest offen. Keine Annahme über Senderempfang oder Autostart.

Ein HTTP-200-Status ist keine bestätigte Wiedergabe. Anzeige und Tastenaktivierung
richten sich nach dem zuletzt gelesenen Volumio-Status, nicht nach der letzten Taste.
Das Panel zeigt keine bestätigte aktive Preset-Taste an, solange eine verlässliche
Zuordnung von URI/Metadaten zu den Presets nicht getestet ist.
