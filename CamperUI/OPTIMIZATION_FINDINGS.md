# CamperUI – Optimierungs- & Verbesserungsanalyse (UX, FPS & Stromverbrauch)

**Zielplattform:** Waveshare ESP32-S3-Touch-LCD-4 (240 MHz Dual-Core, 8 MB Octal PSRAM, 16 MB Flash, 480×480 ST7701S RGB-Panel, GT911 Touch, CH32V003 IO-Expander)  
**Untersuchte Version:** `v1.7.2` (ohne `CamperCam`)

---

## Executive Summary

Obwohl in vergangenen Iterationen bereits wichtige Stabilitäts- und Performance-Verbesserungen umgesetzt wurden (siehe [ANALYSIS_EFFICIENCY_STABILITY.md](file:///c:/Users/njest/Documents/projects/CamperUI/ANALYSIS_EFFICIENCY_STABILITY.md)), zeigt die aktuelle Codebasis deutliche Optimierungspotenziale in allen drei Fokusbereichen:

| Fokusbereich | Aktueller Zustand | Optimierungspotenzial | Erwarteter Effekt |
| :--- | :--- | :--- | :--- |
| **1. User Interaction (UX)** | 60 ms künstliche Touch-Release-Latenz, `scroll_limit = 4px` verschluckt Taps, `Speichern & Beenden` trennt immer das WLAN, Wasserwaage aktualisiert nur alle 6 s | Sofortiges Release bei validem GT911-Frame, `scroll_limit = 8..10px`, separater „Zurück"-Button, Fast-Polling für aktive Tabs | **~60 ms schnellere Klick-Reaktion**, keine verschluckten Button-Taps, kein WLAN-Drop beim Verlassen der Settings |
| **2. FPS & Rendering** | Opacity-Layer (`LV_OPA_70`) erzwingen 24 KB PSRAM-Zwischenpuffer, Temps-Animation triggert 240 Float/PSRAM-Operationen pro Frame, unbedingte Style-Invalidierungen jede Sekunde | Verzicht auf Container-`opa` zugunsten von Farb-Mix, inkrementelles Chart-Reveal, konsequente Style-Guards, 180°-Hardware-Flip im ST7701S | **Konstante 50–60 FPS** auch bei Chart-Animationen, **0 % unnötige Redraws** im 1-Sekunden-Takt |
| **3. Stromverbrauch** | `WiFi.setSleep(false)` dauerhaft aktiv, 240 MHz Festtakt, RGB-DMA + PSRAM-Scanout + 500 ms HTTP-Polling laufen bei ausgeschaltetem Display unverändert weiter | Modem-Sleep (`WIFI_PS_MIN_MODEM`), ST7701S `Sleep In` + RGB-Panel-Pause bei Display-Aus, adaptives Polling & CPU-Taktabsenkung (80 MHz) im Standby | **~50–70 % weniger Stromverbrauch im Standby/Screensaver** (~120–180 mA Ersparnis bei 5 V) |

---

## 1. Fokusbereich: User Interaction (UX & Bedienbarkeit)

### 1.1 Künstliche 60 ms Klick-Verzögerung im `TouchReleaseFilter`
- **Fundstelle:** [`touch_release_filter.h`](file:///c:/Users/njest/Documents/projects/CamperUI/touch_release_filter.h#L10-L19) & [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L105-L145)
- **Ursache:**
  In [`my_touchpad_read()`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L92) wird bereits vorbildlich das Hardware-Statusregister `0x814E` des GT911 geprüft (`bool fresh = status >= 0 && (status & 0x80)`). Dennoch erzwingt [`TouchReleaseFilter::update()`](file:///c:/Users/njest/Documents/projects/CamperUI/touch_release_filter.h#L16) selbst bei einem validen neuen Sensor-Frame mit `pressed == false` (`(status & 0x0f) == 0`) eine Wartezeit von **60 ms** (`now - release_at >= 60`), bevor LVGL das Loslassen (`LV_INDEV_STATE_REL`) erfährt.
- **Auswirkung:**
  Jeder Button-Klick und jedes akustische Buzzer-Feedback (`LV_EVENT_CLICKED`) fühlt sich um ca. 60–70 ms verzögert an.
- **Empfehlung:**
  Zwischen *„kein neuer Frame vom GT911 verfügbar"* (`!fresh` → letzten Zustand halten) und *„GT911 meldet explizit 0 Finger"* (`fresh && !pressed`) unterscheiden. Wenn `fresh && !pressed` vorliegt, kann das Release sofort (oder mit minimalem Entprellfenster von 10–15 ms) gemeldet werden.

### 1.2 Zu empfindlicher Scroll-Schwellwert (`scroll_limit = 4`) verschluckt Taps
- **Fundstelle:** [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L317)
- **Ursache:**
  `indev_drv.scroll_limit = 4;` gilt global für den gesamten Touchscreen. Beim Antippen einer Kachel oder eines Buttons im Wohnmobil rollt die Fingerkuppe auf dem kapazitiven 480×480 Panel leicht um 4–6 Pixel.
- **Auswirkung:**
  Auf allen scrollbaren Seiten (`Home`, `Schalter` bei >6 Relais, `Dimmer`, `Victron`, `Einstellungen`) bricht LVGL den Klick häufig ab und interpretiert den Tap als vertikalen Scroll-Vorgang – der Button reagiert nicht.
- **Empfehlung:**
  `indev_drv.scroll_limit` auf **8 bis 10 Pixel** erhöhen. Das horizontale Scrollen der Navigationsleiste bleibt damit genauso flüssig, aber Taps auf Kacheln und Schalter werden deutlich zuverlässiger erkannt.

### 1.3 Einstellungen (`scr_settings`): Kein „Zurück/Abbrechen" & unnötiger WLAN-Reset
- **Fundstelle:** [`ui_settings.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_settings.cpp#L378-L404) & [`ui_settings.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_settings.cpp#L1551-L1645)
- **Ursache:**
  1. Der Einstellungsbildschirm besitzt unten nur einen einzigen Button `Speichern & Beenden`. Wer die Einstellungen nur öffnet, um den QR-Code zu scannen oder den WLAN-Status zu prüfen, kann die Ansicht nicht ohne Speichern verlassen.
  2. Beim Klick auf `Speichern & Beenden` ruft [`save_settings_cb()`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_settings.cpp#L397-L398) **immer** `WiFi.disconnect(); wifi_connect_configured();` sowie `state_save()` (ca. 80 NVS-Keys) auf – selbst wenn der Nutzer nur einen Temperatursensor oder eine Warnschwelle geändert hat.
- **Auswirkung:**
  Jedes Verlassen der Einstellungen kappt die laufende WLAN-, HTTP- und Victron-MQTT-Verbindung für mehrere Sekunden und beschreibt den Flash-Speicher.
- **Empfehlung:**
  - Unten links einen **„Abbrechen / Zurück"**-Button ergänzen, der ohne NVS-Write und ohne UI-Rebuild direkt zu `scr_main` zurückkehrt.
  - In `save_settings_cb()` `WiFi.disconnect()` / `wifi_connect_configured()` **nur dann** aufrufen, wenn sich `wifi_ssid` oder `wifi_pass` tatsächlich geändert haben.

### 1.4 Wasserwaage (`TAB_LEVEL`) aktualisiert im Live-Betrieb nur alle 6 Sekunden
- **Fundstelle:** [`http_handler.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/http_handler.cpp#L1054-L1084) & [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L459)
- **Ursache:**
  `http_loop()` arbeitet 12 Poll-Schritte im 500-ms-Takt ab. `/position_sensor/?request=true` wird nur in `poll_step == 7` abgefragt, also exakt **einmal alle 6 Sekunden**. Zudem wird `ui_update_visible_page()` in `loop()` nur alle 1000 ms aufgerufen.
- **Auswirkung:**
  Wenn man mit dem Camper auf Auffahrkeile fährt, hinkt die Dosenlibelle und die Zentimeter-Anzeige bis zu 6 Sekunden hinterher.
- **Empfehlung:**
  Solange `active_page == TAB_LEVEL` sichtbar ist, sollte der Background-Worker `/position_sensor/?request=true` priorisiert alle **400–500 ms** abfragen und die UI-Aktualisierung für die Wasserwaage mit 4–5 Hz (alle 200–250 ms) laufen.

### 1.5 Dimmer-Bedienung: Kein Live-Feedback an der Lampe & fehlender Quick-Toggle
- **Fundstelle:** [`ui_switches.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_switches.cpp#L318-L342)
- **Ursache:**
  `dimmer_slider_event_cb()` sendet den neuen Dimmwert erst bei `LV_EVENT_RELEASED` (`http_publish_dimmer`). Während des Ziehens verändert sich zwar der Balken auf dem Display, die echte Beleuchtung im Camper reagiert aber erst nach dem Loslassen.
- **Empfehlung:**
  - **Gedrosseltes Live-Dimming beim Ziehen:** Während `LV_EVENT_VALUE_CHANGED` alle 150–200 ms (bei $\Delta \ge 3\,\%$) ein Update in die `http_cmd_queue` stellen (bzw. in der Queue den letzten ausstehenden Dimmer-Befehl überschreiben).
  - **Tap-to-Toggle auf dem Dimmer-Namen:** Ein Klick auf das Namens-Label links in der Dimmer-Kachel (`lbl_dim_name[i]`) könnte den Dimmer analog zu den Home-Favoriten zwischen `0 %` und dem letzten gespeicherten Helligkeitswert umschalten.

### 1.6 Fehlendes Feedback bei Schaltersperre (2-Sekunden-Hold) & auf dem Wasser-Tab
- **Fundstelle:** [`ui_switches.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_switches.cpp#L105-L114) & [`ui_water.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_water.cpp#L26-L63)
- **Ursache:**
  1. Wenn ein Schalter durch 2 Sekunden Halten gesperrt/entsperrt wird (`LV_EVENT_PRESSING`), ertönt kein Buzzer-Signal (da `my_touchpad_feedback` nur auf `LV_EVENT_CLICKED` reagiert) und das Schloss-Symbol unter dem Finger wird oft verdeckt.
  2. Wenn das Pumpen- oder Abwasser-Relais gesperrt ist (`state.switch_locked[relay]`), zeigt der Button auf dem **Wasser-Tab** (`TAB_WATER`) weder ein `MDI_LOCK`-Icon noch einen Hinweis an – ein Klick wird in [`toggle_water_relay()`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_water.cpp#L28) kommentarlos ignoriert.
- **Empfehlung:**
  Beim Umschalten der Sperre in `relay_btn_event_cb` / `wrelay_btn_event_cb` ein kurzes `buzzer_beep(35)` auslösen und auf dem Wasser-Tab bei gesperrtem Pumpen-/Abwasser-Relais ebenfalls `MDI_LOCK` im Button-Text anzeigen.

---

## 2. Fokusbereich: FPS & Rendering-Performance

### 2.1 Teure LVGL „Simple Layer"-Allokationen durch Container-Opacity (`style_opa`)
- **Fundstelle:** [`ui_temps.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_temps.cpp#L550-L552) & [`ui_settings.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_settings.cpp#L226)
- **Ursache:**
  In [`ui_temps.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_temps.cpp#L551) wird auf die gesamten Sensor-Kacheln `lv_obj_set_style_opa(cards[i], is_vis ? LV_OPA_COVER : LV_OPA_70, 0);` angewendet (und in `ui_settings.cpp` `LV_OPA_40` auf `card_shore_toggle` / `card_alt_toggle`).
  In LVGL v8 zwingt `style_opa < 255` auf einem Container mit Kind-Elementen die Render-Engine dazu, einen temporären **Simple Layer Buffer** (`LV_LAYER_SIMPLE_BUF_SIZE = 24 KB` im langsameren PSRAM) anzulegen, die Kachel samt Schriften dorthin zu rendern und anschließend per Software-Alpha-Blending zurück in den internen Draw-Buffer zu mischen.
  Noch gravierender: In `ui_temps.cpp:545-552` wird `lv_obj_set_style_opa` **jede Sekunde bedingungslos** für alle 4 Kacheln aufgerufen!
- **Auswirkung:**
  Massive CPU-Last und PSRAM-Bandbreitenverbrauch bei jedem Redraw der Temperatur- und Einstellungs-Seite.
- **Empfehlung:**
  Niemals `lv_obj_set_style_opa()` auf Container anwenden. Stattdessen die Text- und Rahmenfarben gedimmter Kacheln direkt auf `ui_theme_muted()` setzen (wie es für `lbl_vals[i]` ohnehin schon gemacht wird).

### 2.2 Temperatur-Chart Reveal-Animation (`ui_trigger_temps_anim`) bremst sich selbst aus
- **Fundstelle:** [`ui_temps.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_temps.cpp#L88-L116) & [`ui_temps.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_temps.cpp#L563-L643)
- **Ursache:**
  Während der 750 ms langen Einblend-Animation (`temps_anim_cb`) wird in jedem Animations-Tick die komplette Funktion `ui_update_temps_tab()` aufgerufen:
  1. Bei jedem Frame werden erst alle 4 Sensor-Kacheln oberhalb des Charts neu berechnet, `snprintf` ausgeführt, Labels per `lv_obj_align()` neu ausgerichtet und Styles gesetzt.
  2. Danach ruft `update_track` **4× pro Frame** `temp_history_get_series()` auf – das sind pro Frame **240 `roundf()`-Berechnungen und PSRAM-Zugriffe** sowie 240 Aufrufe von `lv_chart_set_value_by_id()`.
  3. Durch `bool chart_needs_update = temps_animating || ...` (Zeile 563) wird das Chart selbst dann komplett neu berechnet und per `lv_chart_refresh(chart)` invalidiert, wenn sich `points_to_show` zwischen zwei 10-ms-Ticks gar nicht geändert hat!
- **Auswirkung:**
  Die Reveal-Animation ruckelt spürbar, weil pro Frame hunderte redundante Operationen ausgeführt werden.
- **Empfehlung:**
  - In Zeile 563 `temps_animating` aus dem `||`-Ausdruck entfernen, sodass während der Animation **nur** bei `(points_to_show != last_rendered_points_to_show)` neu gezeichnet wird.
  - Die 4 Serien-Arrays (`points[4][60]`) beim Start der Animation einmalig aus `temp_history_get_series()` zwischenspeichern und im Animations-Callback nur die neu hinzugekommenen Punkte in das `lv_chart` schreiben, ohne die 4 Sensor-Cards oben anzufassen.

### 2.3 Unbedingte Style- & Layout-Invalidierungen im 1-Sekunden-Intervall
- **Fundstelle:** Diverse `ui_*.cpp` Dateien
- **Ursache:**
  Während für Text-Labels bereits `ui_label_set_text_if_changed()` genutzt wird, rufen mehrere Seiten bei jedem 1-Sekunden-Tick (`ui_update_visible_page()`) bedingungslos LVGL-Style-Setter auf. In LVGL prüft `lv_obj_set_style_*` oft nicht, ob der alte Wert identisch ist, sondern markiert das Objekt sofort als „dirty" (inkl. Layout-/Redraw-Triggern):
  - **[`ui_screensaver.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_screensaver.cpp#L89-L106):** Nutzt `lv_label_set_text()` statt `ui_label_set_text_if_changed()` für `lbl_time` (`ui_font_clock_120`!), `lbl_ttgo` und `lbl_sub`. Dadurch wird die riesige 120-Pixel-Uhrzeit **jede Sekunde komplett neu gerendert**, obwohl sich der String `%02d:%02d` nur alle 60 Sekunden ändert!
  - **[`ui_power.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_power.cpp#L172-L356):** Ruft jede Sekunde bedingungslos `lv_arc_set_value()` auf `arc_solar`, `arc_shore`, `arc_alt`, `lv_obj_set_style_line_color()` auf allen 4 Fluss-Linien, `lv_obj_set_style_arc_color(arc_battery, ...)` sowie `lv_obj_set_style_border_color(card_bat_hub, ...)` auf.
  - **[`ui_truma.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_truma.cpp#L461-L550):** Setzt jede Sekunde bedingungslos die Hintergrund- und Rahmenfarben von `dot_online`, `badge_230v` und allen 3 Warmwasser-Buttons (`style_segmented_button`).
  - **[`ui_maxxfan.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_maxxfan.cpp#L80-L81):** Obwohl in Zeile 67 ein `if (mode != last_maxxfan_mode)`-Guard eingebaut wurde, stehen in Zeile 80–81 noch einmal **unbedingte** Aufrufe von `lv_obj_set_pos(lid_image, ...)` und `lv_obj_set_y(info, ...)` außerhalb des `if`-Blocks!
- **Empfehlung:**
  Einen kleinen Inline-Helper `ui_bg_color_if_changed()`, `ui_border_color_if_changed()` und `ui_arc_value_if_changed()` in [`ui_main.h`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_main.h) ergänzen und in `ui_screensaver.cpp` konsequent `ui_label_set_text_if_changed()` verwenden.

### 2.4 Übermäßiger Cache-WriteBack bei partiellen Updates & Software-Rotation
- **Fundstelle:** [`CamperRGBDisplay.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperRGBDisplay.cpp#L392-L470) & [`CamperRGBDisplay.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperRGBDisplay.cpp#L579-L591)
- **Ursache:**
  1. **Cache-Flush über volle Bildschirmbreite:** Wenn LVGL einen schmalen Bereich der Breite `w` und Höhe `h` aktualisiert (z. B. die 30×30 px Blase der Wasserwaage oder ein kurzes Label), schreibt `Cache_WriteBack_Addr(cachePos, _fb_width * h * 2)` bei `_rotation == 2` immer die **vollen 480 Pixel Breite (`_fb_width`)** für alle `h` Zeilen aus dem Data-Cache in das PSRAM zurück.
  2. **180°-Rotation in Software statt Hardware:** Da `rotation = 2` genutzt wird, muss die CPU jeden Pixel-Streifen in [`fast_draw_bitmap_rotate_2()`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperRGBDisplay.cpp#L392) rückwärts lesen und wortweise vertauschen. Der ST7701S-Displaycontroller unterstützt die 180°-Spiegelung jedoch **direkt in Hardware** über die Init-Sequenz (Register `0x36` MADCTL / `0xC7` SDIR).
- **Empfehlung:**
  - Bei Nutzung der Hardware-Spiegelung im ST7701S kann der Framebuffer mit `_rotation = 0` betrieben werden, wodurch `fast_draw_bitmap_rotate_0()` direkt ein hochoptimiertes `memcpy()` nutzt.
  - Falls `_rotation == 2` in Software bleibt: Für schmale Regionen (`w < 240`) zeilenweise `Cache_WriteBack_Addr` nur über `w * 2` Bytes ausführen oder die Startadresse auf die tatsächliche Ziel-Spalte `dst_x_start` begrenzen (`(_fb_width * (h - 1) + w) * 2` analog zu `case 0` in Zeile 588–589!).

### 2.5 Allokationsreihenfolge der internen LVGL Draw-Buffer & WebServer auf Core 1
- **Fundstelle:** [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L256-L282), [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L474) & [`ui_settings.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_settings.cpp#L29-L61)
- **Ursache:**
  1. In `setup()` werden zuerst WiFi und die Hintergrund-Tasks (`http_start_task()`, `victron_init()`) gestartet und erst danach die internen DMA/SRAM-Draw-Buffer `buf1` und `buf2` allokiert. Dadurch ist der interne SRAM bereits fragmentiert, was das Risiko erhöht, dass `buf2` fehlschlägt oder auf 16 Zeilen zurückfällt.
  2. `web_ota_loop()` (`server.handleClient()`) läuft auf Core 1 im UI-Loop. Ein Abruf von `/settings` (66 KB HTML) oder `/api/log` blockiert die UI-Darstellung spürbar. Zudem führt `run_http_fetch()` in `ui_settings.cpp:29` 5 synchrone HTTP-Requests direkt im UI-Event-Callback auf Core 1 aus.
- **Empfehlung:**
  `buf1` und `buf2` direkt vor `wifi_connect_configured()` im internen SRAM reservieren und `server.handleClient()` in einen Core-0-Task verlagern.

---

## 3. Fokusbereich: Stromverbrauch des Systems (Power Consumption)

Im Camper-Einsatz (Autarkie über Aufbaubatterie, 24/7-Dauerbetrieb) ist der Ruhe- und Betriebs-Stromverbrauch entscheidend. Aktuell zieht das System im „Display Aus"- oder Screensaver-Zustand fast genauso viel Strom (abzüglich Backlight) wie unter Volllast.

### 3.1 WLAN Modem-Sleep ist hart deaktiviert (`WiFi.setSleep(false)`)
- **Fundstelle:** [`wifi_diagnostics.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/wifi_diagnostics.cpp#L63)
- **Ursache:**
  In `wifi_connect_configured()` wird `WiFi.setSleep(false);` gesetzt. Damit bleibt der 2,4-GHz-HF-Empfänger des ESP32-S3 zu 100 % der Zeit im aktiven Empfangsmodus (~110–130 mA Konstantstrom allein für das WLAN-Modul), anstatt zwischen den DTIM-Beacons des Routers zu schlafen.
- **Auswirkung:**
  Ca. **0,35 bis 0,5 W dauerhafter Mehrverbrauch** (rund 8–12 Wh bzw. ~0,8–1 Ah pro Tag aus der 12V-Batterie allein für das gehaltene WLAN-Radio).
- **Empfehlung:**
  - Standardmäßig `WiFi.setSleep(WIFI_PS_MIN_MODEM);` (bzw. `WiFi.setSleep(true)`) aktivieren.
  - Alternativ adaptiv steuern: Sobald das Display aktiv bedient wird oder ein Web-OTA-Update läuft (`state.web_ota_active`), `WiFi.setSleep(false)` für minimale Latenz setzen; sobald das Display im Timeout/Screensaver ist, auf `WIFI_PS_MIN_MODEM` oder `WIFI_PS_MAX_MODEM` umschalten.

### 3.2 RGB-DMA, PSRAM-Scanout & ST7701S laufen bei ausgeschaltetem Display weiter
- **Fundstelle:** [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L374-L383) & [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L129-L140)
- **Ursache:**
  Wenn das Display nach Ablauf von `state.display_timeout` ausgeht (`!state.screensaver_enabled`), schaltet Zeile 381 lediglich die Hintergrundbeleuchtung über den CH32V003 ab (`WS_CH32_IO::setPwm(Wire, 255)`):
  1. Der **ST7701S LCD-Controller** bleibt voll im aktiven Betriebsmodus (Ladungspumpen & Gate-Treiber aktiv).
  2. Der **ESP32-S3 LCD-RGB-Controller und GDMA** streamen weiterhin ununterbrochen mit 12 MHz Pixeltakt (24 MB/s!) das 480×480 Bild aus dem Octal-PSRAM an das unsichtbare Panel.
  3. LVGL ruft weiterhin alle 10 ms `my_touchpad_read()` über I2C auf (`100 Hz`) und `loop()` läuft mit maximal 5 ms Delay weiter.
- **Auswirkung:**
  Unnötiger Stromverbrauch von ca. **40–60 mA** durch permanenten PSRAM-Burst-Read, GDMA-Interrupts und ST7701S-Treiber bei dunklem Bildschirm.
- **Empfehlung:**
  - Beim Abschalten des Displays (`display_is_on = false`): ST7701S per SPI in den Sleep-Modus versetzen (`bus->sendCommand(0x28); delay(20); bus->sendCommand(0x10);`) und beim Aufwecken mit `0x11` (Sleep Out) + `0x29` (Display On) reaktivieren.
  - Solange `display_is_on == false`: Die `loop()`-Schleife auf 40–50 ms bremsen (`delay(40)`), sodass der GT911-Touch nur noch mit ~20–25 Hz auf einen Wakeup-Touch abgefragt wird.

### 3.3 Statischer 240 MHz CPU-Takt rund um die Uhr
- **Fundstelle:** [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L202)
- **Ursache:**
  `setCpuFrequencyMhz(240);` fixiert beide Xtensa-LX7-Kerne dauerhaft auf 240 MHz – auch nachts, wenn stundenlang niemand auf das Display schaut oder nur der Screensaver einmal pro Minute die Uhrzeit wechselt.
- **Auswirkung:**
  Ein ESP32-S3 auf 240 MHz benötigt signifikant mehr Ruhestrom als auf 80 MHz oder mit aktiviertem Dynamic Frequency Scaling (DFS).
- **Empfehlung:**
  Wenn `display_is_on == false` oder `ui_screensaver_is_active() == true`, kann die CPU-Frequenz (sofern die RGB-Pixelclock über `LCD_CLK_SRC_PLL160M` unabhängig vom CPU-Takt gespeist wird, siehe [`CamperRGBPanel.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperRGBPanel.cpp#L50)) oder zumindest die Polling-/Wakeup-Rate drastisch reduziert werden. Zudem sollte im Screensaver-Modus [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L478) nicht auf `wait_ms = 5` ms begrenzt werden, sondern `delay(min(wait_ms, 50U))` erlauben, da im Screensaver keinerlei Animationen laufen.

### 3.4 Unnötiges 500-ms-HTTP-Polling im Standby & auf nicht vorhandenen Endpoints
- **Fundstelle:** [`http_handler.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/http_handler.cpp#L1054-L1107)
- **Ursache:**
  1. **Kein Standby-Throttling:** `http_loop()` feuert alle 500 ms einen neuen TCP/HTTP-Request an VanPi ab (12 Requests alle 6 Sekunden = **14.400 HTTP-Requests pro Tag**), selbst wenn das Display komplett ausgeschaltet oder im Screensaver ist. Das hält sowohl das WLAN-Modul des ESP32 als auch den Raspberry Pi (VanPi / Node-RED) permanent wach.
  2. **Endloses Polling auf deaktivierte / 404-Endpoints:** In den Schritten `case 10` (`fetch_shore_data()`) und `case 11` (`fetch_booster_data()`) prüft der Live-Modus **nicht**, ob `state.power_show_shore` bzw. `state.power_show_alt` überhaupt aktiviert sind! Selbst wenn der Server bereits mehrfach `404 Not Found` geliefert hat (`shore_404_count >= 2`), werden `/shore` und `/booster` alle 6 Sekunden weiter abgefragt.
  3. **Inaktive Tabs:** `/level`, `/relay`, `/dimmer`, `/wrelay` werden auch dann alle 6 Sekunden abgefragt, wenn das Display dunkel ist (für die Hintergrund-Historie `temp_history` und `energy_history` werden im 60-Sekunden-Takt lediglich `/batt`, `/mppt/` und `/temp` benötigt).
- **Empfehlung:**
  - Wenn `shore_404_count >= 3` oder `!state.power_show_shore`, das Polling von `/shore` komplett überspringen (analog für `/booster`).
  - Bei ausgeschaltetem Display (`!display_is_on`) oder aktivem Screensaver das Poll-Intervall von `500 ms` auf **`2500–5000 ms`** erhöhen und auf die für Alarme/Verlauf nötigen Endpunkte (`/batt`, `/mppt/`, `/temp`, `/info`) beschränken.

### 3.5 Dauerhaftes Formatieren & Senden von Diagnose-Logs alle 6 Sekunden
- **Fundstelle:** [`http_handler.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/http_handler.cpp#L1086-L1102)
- **Ursache:**
  Bei jedem Durchlauf von `poll_step == 7` (alle 6 Sekunden) werden vier lange `Serial.printf("[CAMPER_UART] ...")`-Zeilen formatiert, in den Ringbuffer kopiert und über den USB-CDC-Stack ausgegeben.
- **Empfehlung:**
  Diese periodischen Telemetrie-Dumps hinter ein Debug-Flag schalten oder nur bei tatsächlichen Wertänderungen / auf expliziten Befehl im Web-Terminal ausgeben.

### 3.6 Logarithmische Helligkeitskurve & Nacht-Absenkung für das Backlight
- **Fundstelle:** [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L225), [`system_state.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/system_state.cpp#L83) & [`ui_screensaver.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_screensaver.cpp#L15-L22)
- **Ursache:**
  Die Hintergrundbeleuchtung ist mit Abstand der größte Einzelverbraucher des Systems (~250–350 mA bei 100 %).
  1. In `system_state.cpp:83` ist der Standardwert `display_brightness = 100`.
  2. Die Umrechnung `255 - (state.display_brightness * 255 / 100)` ist streng linear (und beachtet außerhalb des Screensavers nicht einmal den Hardware-Cutout des Boost-Converters bei PWM > 230, der in `ui_screensaver_brightness_to_pwm()` bereits dokumentiert ist!). Da das menschliche Auge Helligkeit logarithmisch wahrnimmt, wirken 80 % und 100 % linear fast gleich hell, verbrauchen aber deutlich unterschiedlich viel Strom.
- **Empfehlung:**
  - Überall einheitlich `ui_screensaver_brightness_to_pwm()` mit einer leicht quadratischen/logarithmischen Gamma-Kurve ($\gamma \approx 2{,}0$) verwenden.
  - Den Default-Wert von `100 %` auf `75–80 %` senken (~25 % Backlight-Stromersparnis ohne sichtbaren Kontrastverlust).

---

## 4. Priorisierte Umsetzungs-Roadmap (Übersicht)

| Prio | Maßnahme | Bereich | Datei(en) | Aufwand |
| :---: | :--- | :--- | :--- | :---: |
| **1** | `ui_label_set_text_if_changed()` im Screensaver (120px Font nicht jede Sekunde neu rendern) | FPS / Strom | [`ui_screensaver.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_screensaver.cpp#L81-L107) | Sehr gering |
| **2** | Container-Opacity (`lv_obj_set_style_opa`) in `ui_temps.cpp` entfernen + `temps_animating`-Guard in Zeile 563 korrigieren | FPS | [`ui_temps.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_temps.cpp#L550-L567) | Sehr gering |
| **3** | `WiFi.setSleep(true)` (Modem-Sleep) aktivieren statt dauerhaftem `setSleep(false)` | Strom | [`wifi_diagnostics.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/wifi_diagnostics.cpp#L63) | Sehr gering |
| **4** | `TouchReleaseFilter` bei `fresh && !pressed` sofort freigeben + `scroll_limit` auf `8` erhöhen | UX | [`touch_release_filter.h`](file:///c:/Users/njest/Documents/projects/CamperUI/touch_release_filter.h#L10-L19), [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L317) | Gering |
| **5** | „Zurück"-Button in Settings + WLAN nur bei geänderter SSID/Passwort neu verbinden | UX | [`ui_settings.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_settings.cpp#L378-L404) | Gering |
| **6** | HTTP-Polling bei Display-Aus / Screensaver drosseln & `/shore` + `/booster` bei Deaktivierung/404 überspringen | Strom | [`http_handler.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/http_handler.cpp#L779-L872), [`http_handler.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/http_handler.cpp#L1054-L1084) | Gering |
| **7** | ST7701S `Sleep In (0x10)` und Loop-Pause (`40 ms`) bei ausgeschaltetem Display | Strom | [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L374-L383), [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L476-L482) | Mittel |
| **8** | Fast-Polling (`500 ms`) für `/position_sensor/` wenn `active_page == TAB_LEVEL` | UX | [`http_handler.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/http_handler.cpp#L1068), [`CamperUI.ino`](file:///c:/Users/njest/Documents/projects/CamperUI/CamperUI.ino#L459) | Mittel |
| **9** | Style-Change-Guards in `ui_power.cpp`, `ui_truma.cpp` und `ui_maxxfan.cpp` ergänzen | FPS / Strom | [`ui_power.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_power.cpp), [`ui_truma.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_truma.cpp), [`ui_maxxfan.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_maxxfan.cpp) | Gering |
| **10** | Gedrosseltes Live-Publishing beim Ziehen der Dimmer-Slider + Buzzer-Feedback beim 2s-Lock | UX | [`ui_switches.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_switches.cpp#L105-L114), [`ui_switches.cpp`](file:///c:/Users/njest/Documents/projects/CamperUI/ui_switches.cpp#L318-L342) | Gering |

