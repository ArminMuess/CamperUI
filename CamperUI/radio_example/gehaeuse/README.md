# Radio-Blende: Druckteile (offene Bauweise)

Alle Teile werden einzeln von hinten auf die 10-mm-Holzblende geschraubt.
Erzeugt mit `blende_teile.py` (manifold3d). Maße noch nicht am echten Aufbau geprüft:
Stapelhöhe `STACK_H` (geschätzt 45 mm innen) und Stick-Auflage nach dem Nachmessen anpassen.

| Teil | Größe (mm) | Schrauben |
|---|---|---|
| `display_haube` | 101 × 103 × 14,5 | 4 × Holzschraube 3,5 × 10, 4 × M2,5 × 6 (Display) |
| `pi_gehaeuse` + `pi_deckel` | 100 × 76 × 50 (+ Zunge 62 mm für den Stick) | 5 × 3,5 × 10, 4 × M2,5 × 6 (Pi), 1 Kabelbinder (Stick), 4 × M3 × 8 Senkkopf (Deckel) |
| `drehgeber_platte` | 52 × 22 | 2 × 3,5 × 10, Mutter M7 des Drehgebers |
| `wago_halter` | 98 × 21 | 2 × 3,5 × 10, 2 Kabelbinder |
| `shelly_halter` | 79 × 40 | 2 × 3,5 × 10, 2 Kabelbinder |

Druck: PETG, 0,2 mm, 4 Wände, 25–40 % Infill, ohne Stützen in der gespeicherten Lage.

Blende fräsen: Display-Durchbruch 78 × 80 (ohne Aussparung, das Glas liegt mit dem 1-mm-Klebeband auf),
Drehgeber Ø 6,5 durch + von hinten Tasche Ø 12 × 6 tief für Gewindehülse und Mutter.
Dome der Haube enden 1,0 mm hinter der Blende (11,2 mm aus dem Dev-Case − 10 mm Holz − 0,2 mm Vorspannung);
liegt das Glas nicht satt auf, Unterlegscheiben M2,5 zwischen Buchse und Dom.
