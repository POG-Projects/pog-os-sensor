#pragma once

#include <cmath>
#include <cstdint>

namespace pogsensor {

struct Reading {
  float temperature = NAN;
  float humidity = NAN;
  float pressure = NAN;
  float co2 = NAN;
  float illuminance = NAN;
  float vocIndex = NAN;
  float gasResistance = NAN;
  int32_t wifiRssi = 0;
  bool sensorOnline = false;
  bool hasTemperature = false;
  bool hasHumidity = false;
  bool hasPressure = false;
  bool hasCo2 = false;
  bool hasIlluminance = false;
  bool hasVocIndex = false;
  bool hasGasResistance = false;
};

inline bool finiteReading(const Reading &reading) {
  const bool hasMeasurement = reading.hasTemperature || reading.hasHumidity ||
                              reading.hasPressure || reading.hasCo2 ||
                              reading.hasIlluminance || reading.hasVocIndex ||
                              reading.hasGasResistance;
  return reading.sensorOnline && hasMeasurement &&
         (!reading.hasTemperature || std::isfinite(reading.temperature)) &&
         (!reading.hasHumidity || std::isfinite(reading.humidity)) &&
         (!reading.hasPressure || std::isfinite(reading.pressure)) &&
         (!reading.hasCo2 || std::isfinite(reading.co2)) &&
         (!reading.hasIlluminance || std::isfinite(reading.illuminance)) &&
         (!reading.hasVocIndex || std::isfinite(reading.vocIndex)) &&
         (!reading.hasGasResistance ||
          std::isfinite(reading.gasResistance));
}

inline bool materiallyChanged(const Reading &previous, const Reading &next) {
  if (previous.sensorOnline != next.sensorOnline ||
      previous.hasTemperature != next.hasTemperature ||
      previous.hasHumidity != next.hasHumidity ||
      previous.hasPressure != next.hasPressure ||
      previous.hasCo2 != next.hasCo2 ||
      previous.hasIlluminance != next.hasIlluminance ||
      previous.hasVocIndex != next.hasVocIndex ||
      previous.hasGasResistance != next.hasGasResistance) {
    return true;
  }
  if (!next.sensorOnline) return false;
  if (!finiteReading(previous) || !finiteReading(next)) return true;
  return (next.hasTemperature &&
          std::fabs(previous.temperature - next.temperature) >= 0.2f) ||
         (next.hasHumidity &&
          std::fabs(previous.humidity - next.humidity) >= 1.0f) ||
         (next.hasPressure &&
          std::fabs(previous.pressure - next.pressure) >= 1.0f) ||
         (next.hasCo2 && std::fabs(previous.co2 - next.co2) >= 25.0f) ||
         (next.hasIlluminance &&
          std::fabs(previous.illuminance - next.illuminance) >= 10.0f) ||
         (next.hasVocIndex &&
          std::fabs(previous.vocIndex - next.vocIndex) >= 5.0f) ||
         (next.hasGasResistance &&
          std::fabs(previous.gasResistance - next.gasResistance) >= 2.0f) ||
         std::abs(previous.wifiRssi - next.wifiRssi) >= 5;
}

}  // namespace pogsensor
