#pragma once

#include <cmath>
#include <cstdint>

namespace pogsensor {

struct Reading {
  float temperature = NAN;
  float humidity = NAN;
  float pressure = NAN;
  int32_t wifiRssi = 0;
  bool sensorOnline = false;
  bool hasHumidity = false;
};

inline bool finiteReading(const Reading &reading) {
  return reading.sensorOnline && std::isfinite(reading.temperature) &&
         std::isfinite(reading.pressure) &&
         (!reading.hasHumidity || std::isfinite(reading.humidity));
}

inline bool materiallyChanged(const Reading &previous, const Reading &next) {
  if (previous.sensorOnline != next.sensorOnline ||
      previous.hasHumidity != next.hasHumidity) {
    return true;
  }
  if (!next.sensorOnline) return false;
  if (!finiteReading(previous) || !finiteReading(next)) return true;
  return std::fabs(previous.temperature - next.temperature) >= 0.2f ||
         std::fabs(previous.pressure - next.pressure) >= 1.0f ||
         (next.hasHumidity &&
          std::fabs(previous.humidity - next.humidity) >= 1.0f) ||
         std::abs(previous.wifiRssi - next.wifiRssi) >= 5;
}

}  // namespace pogsensor
