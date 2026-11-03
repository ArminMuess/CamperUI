#pragma once
#include <stdint.h>
// Signed watts retain charge/discharge direction. INT16_MIN means missing.
struct EnergySample { int16_t value[3]; };
class EnergyHistoryBuffer {
public:
    static constexpr int capacity = 1440;
    static constexpr int16_t missing = INT16_MIN;
    EnergySample samples[capacity] = {};
    int head = 0, count = 0;
    constexpr void reset() { head = count = 0; }
    constexpr void push(EnergySample sample) {
        samples[head] = sample; head = (head + 1) % capacity;
        if (count < capacity) ++count;
    }
    constexpr int16_t ago(int track, int offset) const {
        return track < 0 || track > 2 || offset < 0 || offset >= count ? missing :
            samples[(head - 1 - offset + capacity) % capacity].value[track];
    }
    // Every bucket covers a disjoint part of the requested time window.
    constexpr int16_t bucket(int track, int minutes, int points, int point) const {
        if (minutes < points || points < 1 || point < 0 || point >= points) return missing;
        int first = (points - 1 - point) * minutes / points;
        int end = (points - point) * minutes / points;
        // The first bucket after startup may contain fewer recorded minutes.
        // Missing recorded minutes still break the curve.
        if (end > count) end = count;
        if (end <= first) return missing;
        int32_t sum = 0; int valid = 0;
        for (int offset = first; offset < end; ++offset) {
            int16_t v = ago(track, offset);
            if (v != missing) { sum += v; ++valid; }
        }
        // Do not draw across missing recorded minutes.
        return valid == end - first ? (int16_t)(sum / valid) : missing;
    }
};
