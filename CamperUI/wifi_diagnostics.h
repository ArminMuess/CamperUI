#pragma once
#include <Arduino.h>
void wifi_diagnostics_init();
void wifi_connect_configured();
void wifi_diagnostics_loop();
void wifi_set_scanning(bool scanning);
bool wifi_is_scanning();
void wifi_prepare_for_scan();
void wifi_finish_scan();
String wifi_connection_summary();
String wifi_connection_details();
