#pragma once
#include <Arduino.h>

enum class FanAction : uint8_t { Mode, Speed, Temperature, Lid, Direction };
struct FanStatus {
    bool valid = false, demo = false, power = false, automatic = false;
    bool lid_open = false, intake = false, pending = false;
    int speed = 3, target = 26;
    uint32_t updated = 0;
    char message[96] = "Warte auf VanPi";
};
FanStatus maxxfan_status();
bool maxxfan_request(FanAction action, int value);
// Called exclusively by the existing HTTP worker on Core 0; never touches LVGL.
void maxxfan_worker();
