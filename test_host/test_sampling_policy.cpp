#include <cassert>

#include "sampling_policy.h"

using pogsensor::Reading;
using pogsensor::finiteReading;
using pogsensor::materiallyChanged;

Reading climate(float temperature, float humidity, float pressure,
                int32_t rssi = -55) {
  Reading reading;
  reading.temperature = temperature;
  reading.humidity = humidity;
  reading.pressure = pressure;
  reading.wifiRssi = rssi;
  reading.sensorOnline = true;
  reading.hasTemperature = true;
  reading.hasHumidity = true;
  reading.hasPressure = true;
  return reading;
}

int main() {
  Reading base = climate(21.0f, 45.0f, 1013.0f);
  assert(finiteReading(base));
  assert(!materiallyChanged(base, climate(21.1f, 45.5f, 1013.5f, -53)));
  assert(materiallyChanged(base, climate(21.2f, 45.0f, 1013.0f)));
  assert(materiallyChanged(base, climate(21.0f, 46.0f, 1013.0f)));
  assert(materiallyChanged(base, climate(21.0f, 45.0f, 1014.0f)));
  assert(materiallyChanged(base, climate(21.0f, 45.0f, 1013.0f, -60)));

  Reading air = base;
  air.co2 = 800.0f;
  air.illuminance = 100.0f;
  air.hasCo2 = true;
  air.hasIlluminance = true;
  Reading stableAir = air;
  stableAir.co2 = 824.0f;
  stableAir.illuminance = 109.0f;
  assert(!materiallyChanged(air, stableAir));
  stableAir.co2 = 825.0f;
  assert(materiallyChanged(air, stableAir));
  stableAir = air;
  stableAir.illuminance = 110.0f;
  assert(materiallyChanged(air, stableAir));

  Reading gases = base;
  gases.vocIndex = 100.0f;
  gases.gasResistance = 42.0f;
  gases.hasVocIndex = true;
  gases.hasGasResistance = true;
  Reading stableGases = gases;
  stableGases.vocIndex = 104.0f;
  stableGases.gasResistance = 43.9f;
  assert(!materiallyChanged(gases, stableGases));
  stableGases.vocIndex = 105.0f;
  assert(materiallyChanged(gases, stableGases));

  Reading lightOnly;
  lightOnly.sensorOnline = true;
  lightOnly.hasIlluminance = true;
  lightOnly.illuminance = 2.5f;
  assert(finiteReading(lightOnly));

  Reading offline = base;
  offline.sensorOnline = false;
  assert(materiallyChanged(base, offline));
  return 0;
}
