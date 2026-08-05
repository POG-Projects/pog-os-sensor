#include "environment_sensor.h"

#include <Adafruit_AHTX0.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BME680.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_VEML7700.h>
#include <SensirionI2cScd4x.h>
#include <SensirionI2CSgp40.h>
#include <VOCGasIndexAlgorithm.h>
#include <Wire.h>

#include "config.h"

namespace {
Adafruit_BME280 bme;
Adafruit_BME680 bme680(&Wire);
Adafruit_BMP280 bmp;
Adafruit_SHT31 sht;
Adafruit_AHTX0 aht;
Adafruit_VEML7700 veml;
SensirionI2cScd4x scd4x;
SensirionI2CSgp40 sgp40;
VOCGasIndexAlgorithm vocAlgorithm;

bool hasBme = false;
bool hasBme680 = false;
bool hasBmp = false;
bool hasSht = false;
bool hasAht = false;
bool hasScd4x = false;
bool hasVeml = false;
bool hasSgp40 = false;
float latestVocIndex = NAN;
float compensationTemperature = 25.0f;
float compensationHumidity = 50.0f;
uint32_t nextVocSample = 0;
String model;
String addresses;

bool deviceAt(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void addDetected(const char *name, uint8_t address) {
  if (model.length()) model += " + ";
  model += name;
  char formatted[7];
  snprintf(formatted, sizeof(formatted), "0x%02X", address);
  if (addresses.length()) addresses += ", ";
  addresses += formatted;
}

bool tryBme(uint8_t candidate) {
  if (!bme.begin(candidate, &Wire)) return false;
  hasBme = true;
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X2,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::FILTER_X4);
  addDetected("BME280", candidate);
  return true;
}

bool tryBme680(uint8_t candidate) {
  if (!bme680.begin(candidate)) return false;
  hasBme680 = true;
  bme680.setTemperatureOversampling(BME680_OS_8X);
  bme680.setHumidityOversampling(BME680_OS_2X);
  bme680.setPressureOversampling(BME680_OS_4X);
  bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme680.setGasHeater(320, 150);
  addDetected("BME680", candidate);
  return true;
}

bool tryBmp(uint8_t candidate) {
  if (!bmp.begin(candidate)) return false;
  hasBmp = true;
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X1,
                  Adafruit_BMP280::FILTER_X4,
                  Adafruit_BMP280::STANDBY_MS_1000);
  addDetected("BMP280", candidate);
  return true;
}

bool trySht(uint8_t candidate) {
  if (!sht.begin(candidate)) return false;
  hasSht = true;
  addDetected("SHT3x", candidate);
  return true;
}

void detectScd4x() {
  if (!deviceAt(SCD41_I2C_ADDR_62)) return;
  scd4x.begin(Wire, SCD41_I2C_ADDR_62);
  // Le composant peut déjà mesurer après un redémarrage logiciel. La commande
  // stop remet les deux situations dans un état connu avant de relancer le
  // mode périodique (une nouvelle valeur toutes les cinq secondes).
  scd4x.stopPeriodicMeasurement();
  if (scd4x.startPeriodicMeasurement() == 0) {
    hasScd4x = true;
    addDetected("SCD4x", SCD41_I2C_ADDR_62);
  }
}
}  // namespace

bool environmentSensorBegin() {
  hasBme = hasBme680 = hasBmp = hasSht = hasAht = false;
  hasScd4x = hasVeml = hasSgp40 = false;
  latestVocIndex = NAN;
  compensationTemperature = 25.0f;
  compensationHumidity = 50.0f;
  nextVocSample = 0;
  model = "";
  addresses = "";
  Wire.end();
  if (!Wire.begin(g_config.sdaPin, g_config.sclPin)) return false;
  Wire.setClock(100000);

  // Un BME280 couvre à lui seul le climat complet. Sans BME, un SHT/AHT peut
  // être associé à un BMP280 afin de réunir humidité et pression.
  if (!(tryBme(0x76) || tryBme(0x77) ||
        tryBme680(0x76) || tryBme680(0x77))) {
    if (!(trySht(0x44) || trySht(0x45))) {
      if (aht.begin(&Wire)) {
        hasAht = true;
        addDetected("AHT10/20", 0x38);
      }
    }
    tryBmp(0x76) || tryBmp(0x77);
  }

  detectScd4x();
  if (deviceAt(0x10) && veml.begin(&Wire)) {
    hasVeml = true;
    addDetected("VEML7700", 0x10);
  }
  if (deviceAt(0x59)) {
    sgp40.begin(Wire);
    uint16_t serial[3] = {};
    if (sgp40.getSerialNumber(serial, 3) == 0) {
      hasSgp40 = true;
      addDetected("SGP40", 0x59);
    }
  }
  if (!model.length()) model = "aucun capteur compatible";
  return environmentSensorPresent();
}

void environmentSensorLoop() {
  if (!hasSgp40) return;
  uint32_t now = millis();
  if ((int32_t)(now - nextVocSample) < 0) return;
  nextVocSample = now + 1000;

  float humidity = constrain(compensationHumidity, 0.0f, 100.0f);
  float temperature = constrain(compensationTemperature, -45.0f, 130.0f);
  uint16_t humidityTicks = lroundf(humidity * 65535.0f / 100.0f);
  uint16_t temperatureTicks =
      lroundf((temperature + 45.0f) * 65535.0f / 175.0f);
  uint16_t raw = 0;
  if (sgp40.measureRawSignal(humidityTicks, temperatureTicks, raw) == 0) {
    int32_t index = vocAlgorithm.process(raw);
    if (index > 0) latestVocIndex = index;
  }
}

bool environmentSensorRead(pogsensor::Reading &reading) {
  reading.sensorOnline = false;

  if (hasBme) {
    bme.takeForcedMeasurement();
    reading.temperature = bme.readTemperature() + g_config.temperatureOffset;
    reading.humidity = bme.readHumidity();
    reading.pressure = bme.readPressure() / 100.0f;
    reading.hasTemperature = std::isfinite(reading.temperature);
    reading.hasHumidity = std::isfinite(reading.humidity);
    reading.hasPressure = std::isfinite(reading.pressure);
  } else if (hasBme680) {
    if (bme680.performReading()) {
      reading.temperature =
          bme680.temperature + g_config.temperatureOffset;
      reading.humidity = bme680.humidity;
      reading.pressure = bme680.pressure / 100.0f;
      reading.gasResistance = bme680.gas_resistance / 1000.0f;
      reading.hasTemperature = std::isfinite(reading.temperature);
      reading.hasHumidity = std::isfinite(reading.humidity);
      reading.hasPressure = std::isfinite(reading.pressure);
      reading.hasGasResistance =
          std::isfinite(reading.gasResistance) && reading.gasResistance >= 0;
    }
  } else {
    if (hasSht) {
      reading.temperature = sht.readTemperature() + g_config.temperatureOffset;
      reading.humidity = sht.readHumidity();
      reading.hasTemperature = std::isfinite(reading.temperature);
      reading.hasHumidity = std::isfinite(reading.humidity);
    } else if (hasAht) {
      sensors_event_t humidity;
      sensors_event_t temperature;
      aht.getEvent(&humidity, &temperature);
      reading.temperature = temperature.temperature + g_config.temperatureOffset;
      reading.humidity = humidity.relative_humidity;
      reading.hasTemperature = std::isfinite(reading.temperature);
      reading.hasHumidity = std::isfinite(reading.humidity);
    }
    if (hasBmp) {
      bmp.takeForcedMeasurement();
      float bmpTemperature =
          bmp.readTemperature() + g_config.temperatureOffset;
      if (!reading.hasTemperature) {
        reading.temperature = bmpTemperature;
        reading.hasTemperature = std::isfinite(reading.temperature);
      }
      reading.pressure = bmp.readPressure() / 100.0f;
      reading.hasPressure = std::isfinite(reading.pressure);
    }
  }

  if (hasScd4x) {
    bool ready = false;
    if (scd4x.getDataReadyStatus(ready) == 0 && ready) {
      uint16_t co2 = 0;
      float temperature = NAN;
      float humidity = NAN;
      if (scd4x.readMeasurement(co2, temperature, humidity) == 0 && co2 > 0) {
        reading.co2 = co2;
        reading.hasCo2 = true;
        if (!reading.hasTemperature && std::isfinite(temperature)) {
          reading.temperature = temperature + g_config.temperatureOffset;
          reading.hasTemperature = true;
        }
        if (!reading.hasHumidity && std::isfinite(humidity)) {
          reading.humidity = humidity;
          reading.hasHumidity = true;
        }
      }
    }
  }

  if (hasVeml) {
    reading.illuminance = veml.readLux();
    reading.hasIlluminance = std::isfinite(reading.illuminance) &&
                             reading.illuminance >= 0.0f;
  }

  if (reading.hasTemperature) compensationTemperature = reading.temperature;
  if (reading.hasHumidity) compensationHumidity = reading.humidity;
  if (hasSgp40 && std::isfinite(latestVocIndex)) {
    reading.vocIndex = latestVocIndex;
    reading.hasVocIndex = true;
  }

  reading.sensorOnline = reading.hasTemperature || reading.hasHumidity ||
                         reading.hasPressure || reading.hasCo2 ||
                         reading.hasIlluminance || reading.hasVocIndex ||
                         reading.hasGasResistance;
  return pogsensor::finiteReading(reading);
}

const char *environmentSensorModel() { return model.c_str(); }
const char *environmentSensorAddresses() { return addresses.c_str(); }
bool environmentSensorPresent() {
  return hasBme || hasBme680 || hasBmp || hasSht || hasAht || hasScd4x ||
         hasVeml || hasSgp40;
}
bool environmentHasTemperature() {
  return hasBme || hasBme680 || hasBmp || hasSht || hasAht || hasScd4x;
}
bool environmentHasHumidity() {
  return hasBme || hasBme680 || hasSht || hasAht || hasScd4x;
}
bool environmentHasPressure() { return hasBme || hasBme680 || hasBmp; }
bool environmentHasCo2() { return hasScd4x; }
bool environmentHasIlluminance() { return hasVeml; }
bool environmentHasVocIndex() { return hasSgp40; }
bool environmentHasGasResistance() { return hasBme680; }
