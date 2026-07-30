#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "config.h"
#include "environment_sensor.h"
#include "pogdev.h"
#include "sampling_policy.h"
#include "web_portal.h"

#ifndef POGSENSOR_FW_VERSION
#define POGSENSOR_FW_VERSION "dev"
#endif

namespace {
constexpr char kHostname[] = "pogsensor";
constexpr char kSetupSsid[] = "POG-Sensor-Setup";
constexpr char kFirmwareIdentity[] = "POGSensor/" POGSENSOR_FW_VERSION;
bool mdnsStarted = false;
bool portalStarted = false;
bool hasPreviousReading = false;
pogsensor::Reading previousReading;
uint32_t nextSample = 0;
uint32_t connectStarted = 0;

void startSetupAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(kSetupSsid);
  if (!portalStarted) {
    webPortalBegin(true);
    portalStarted = true;
  }
  Serial.printf("[WiFi] configurez %s sur http://192.168.4.1\n", kSetupSsid);
}

void beginStation() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(kHostname);
  WiFi.begin(g_config.wifiSsid.c_str(), g_config.wifiPassword.c_str());
  connectStarted = millis();
}

void onWifiConnected() {
  if (!mdnsStarted && MDNS.begin(kHostname)) {
    mdnsStarted = true;
    MDNS.addService("http", "tcp", 80);
  }
  if (!portalStarted) {
    webPortalBegin(false);
    portalStarted = true;
  }
  Serial.printf("[WiFi] connecté: %s · http://%s.local\n",
                WiFi.localIP().toString().c_str(), kHostname);
}

void sampleEnvironment(uint32_t now) {
  if ((int32_t)(now - nextSample) < 0) return;
  pogsensor::Reading reading;
  reading.wifiRssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  bool ok = environmentSensorRead(reading);
  if (!ok) {
    reading.sensorOnline = false;
    Serial.printf("[Capteur] lecture invalide: T=%f °C P=%f hPa H=%f %%\n",
                  reading.temperature, reading.pressure, reading.humidity);
  } else {
    Serial.printf("[Capteur] %.1f °C · %.1f hPa",
                  reading.temperature, reading.pressure);
    if (reading.hasHumidity) Serial.printf(" · %.1f %%", reading.humidity);
    Serial.println();
  }
  bool changed = !hasPreviousReading ||
                 pogsensor::materiallyChanged(previousReading, reading);
  pogdevSetReading(reading, changed);
  previousReading = reading;
  hasPreviousReading = true;
  nextSample = now + g_config.samplePeriodSeconds * 1000UL;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.printf("\n=== POG Sensor %s ===\n", POGSENSOR_FW_VERSION);
  Serial.printf("[Firmware] %s\n", kFirmwareIdentity);
  configBegin();
  configLoad();
  bool sensorFound = environmentSensorBegin();
  Serial.printf("[Capteur] %s, SDA=%u SCL=%u%s\n",
                environmentSensorModel(), g_config.sdaPin, g_config.sclPin,
                sensorFound ? "" : " (vérifiez le câblage)");
  pogdevBegin();
  if (g_config.wifiSsid.length()) beginStation();
  else startSetupAccessPoint();
}

void loop() {
  uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    static bool announcedConnection = false;
    if (!announcedConnection) {
      announcedConnection = true;
      onWifiConnected();
    }
  } else if (g_config.wifiSsid.length() && !WiFi.softAPIP() &&
             now - connectStarted > 20000) {
    startSetupAccessPoint();
  }
  webPortalLoop();
  sampleEnvironment(now);
  pogdevLoop();
  delay(5);
}
