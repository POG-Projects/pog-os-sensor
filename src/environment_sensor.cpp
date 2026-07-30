#include "environment_sensor.h"

#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>

#include "config.h"

namespace {
Adafruit_BME280 bme;
Adafruit_BMP280 bmp;
SensorKind kind = SensorKind::None;
uint8_t address = 0;

bool tryBmeAddress(uint8_t candidate) {
  if (bme.begin(candidate, &Wire)) {
    kind = SensorKind::Bme280;
    address = candidate;
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X2,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::FILTER_X4);
    return true;
  }
  return false;
}

bool tryBmpAddress(uint8_t candidate) {
  if (bmp.begin(candidate)) {
    kind = SensorKind::Bmp280;
    address = candidate;
    bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X1,
                    Adafruit_BMP280::FILTER_X4,
                    Adafruit_BMP280::STANDBY_MS_1000);
    return true;
  }
  return false;
}
}  // namespace

bool environmentSensorBegin() {
  kind = SensorKind::None;
  address = 0;
  Wire.end();
  if (!Wire.begin(g_config.sdaPin, g_config.sclPin)) return false;
  Wire.setClock(100000);
  // Toujours préférer un BME280 : si deux composants partagent le bus, c'est
  // lui qui fournit les trois mesures attendues par POG Home.
  return tryBmeAddress(0x76) || tryBmeAddress(0x77) ||
         tryBmpAddress(0x76) || tryBmpAddress(0x77);
}

bool environmentSensorRead(pogsensor::Reading &reading) {
  reading.sensorOnline = false;
  reading.hasHumidity = kind == SensorKind::Bme280;
  if (kind == SensorKind::Bme280) {
    // Certaines révisions du pilote perdent le mode mémorisé alors que le
    // composant possède déjà une conversion valide. Les lectures finies
    // ci-dessous restent l'autorité.
    bme.takeForcedMeasurement();
    reading.temperature = bme.readTemperature() + g_config.temperatureOffset;
    reading.humidity = bme.readHumidity();
    reading.pressure = bme.readPressure() / 100.0f;
  } else if (kind == SensorKind::Bmp280) {
    bmp.takeForcedMeasurement();
    reading.temperature = bmp.readTemperature() + g_config.temperatureOffset;
    reading.pressure = bmp.readPressure() / 100.0f;
    reading.humidity = NAN;
  } else {
    return false;
  }
  // La présence du pilote est déjà établie ici ; finiteReading peut maintenant
  // valider uniquement la cohérence des valeurs remontées.
  reading.sensorOnline = true;
  reading.sensorOnline = pogsensor::finiteReading(reading);
  return reading.sensorOnline;
}

SensorKind environmentSensorKind() { return kind; }

const char *environmentSensorModel() {
  switch (kind) {
    case SensorKind::Bme280:
      return "BME280";
    case SensorKind::Bmp280:
      return "BMP280";
    default:
      return "BME/BMP280 absent";
  }
}

uint8_t environmentSensorAddress() { return address; }
