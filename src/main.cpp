#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "config.h"
#include "environment_sensor.h"
#include "ota_update.h"
#include "pogdev.h"
#include "presence_light.h"
#include "radar_sensor.h"
#include "sampling_policy.h"
#include "status_led.h"
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
bool radarWasPresent = false;
pogsensor::Reading previousReading;
uint32_t nextSample = 0;
uint32_t connectStarted = 0;
uint32_t pogHomeDisconnectedSince = 0;
uint32_t otaErrorStarted = 0;
bool environmentReadFault = false;
OtaVisualState previousOtaState = OtaVisualState::Idle;

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
  environmentReadFault = environmentSensorPresent() && !ok;
  if (!ok) {
    reading.sensorOnline = false;
    Serial.println("[Capteur] aucune mesure valide");
  } else {
    Serial.print("[Capteur]");
    if (reading.hasTemperature) Serial.printf(" %.1f °C", reading.temperature);
    if (reading.hasHumidity) Serial.printf(" · %.1f %%", reading.humidity);
    if (reading.hasPressure) Serial.printf(" · %.1f hPa", reading.pressure);
    if (reading.hasCo2) Serial.printf(" · %.0f ppm CO2", reading.co2);
    if (reading.hasIlluminance) {
      Serial.printf(" · %.1f lx", reading.illuminance);
    }
    if (reading.hasVocIndex) Serial.printf(" · VOC %.0f", reading.vocIndex);
    if (reading.hasGasResistance) {
      Serial.printf(" · gaz %.1f kOhm", reading.gasResistance);
    }
    Serial.println();
  }
  bool changed = !hasPreviousReading ||
                 pogsensor::materiallyChanged(previousReading, reading);
  pogdevSetReading(reading, changed);
  previousReading = reading;
  hasPreviousReading = true;
  nextSample = now + g_config.samplePeriodSeconds * 1000UL;
}

void sampleRadar() {
  bool changed = radarSensorLoop();
  bool present = radarSensorPresent();
  if (present && !radarWasPresent) {
    Serial.printf("[Radar] détecté: %s\n", radarSensorModel());
    pogdevRefreshManifest();
  }
  if (changed || present != radarWasPresent) {
    const RadarReading &radar = radarSensorReading();
    Serial.printf("[Radar] %s · %u cible(s) · mouvement %s\n",
                  radar.occupied ? "présence" : "libre", radar.targetCount,
                  radar.motion ? "oui" : "non");
    pogdevSetRadar(radar, true);
  }
  radarWasPresent = present;
}

void updateStatusLed(uint32_t now) {
  if (!g_config.statusLightInstalled) return;
  OtaVisualState ota = otaUpdateVisualState();
  if (ota != previousOtaState) {
    if (ota == OtaVisualState::Error) otaErrorStarted = now;
    previousOtaState = ota;
  }

  StatusLedCode code = StatusLedCode::Ready;
  wifi_mode_t wifiMode = WiFi.getMode();
  bool setupActive = wifiMode == WIFI_MODE_AP || wifiMode == WIFI_MODE_APSTA;

  if (ota == OtaVisualState::Downloading) {
    code = StatusLedCode::OtaDownloading;
  } else if (ota == OtaVisualState::Verifying) {
    code = StatusLedCode::OtaVerifying;
  } else if (ota == OtaVisualState::Checking) {
    code = StatusLedCode::OtaChecking;
  } else if (ota == OtaVisualState::Error &&
             now - otaErrorStarted < 12000) {
    code = StatusLedCode::OtaError;
  } else if (setupActive) {
    code = StatusLedCode::Setup;
  } else if (WiFi.status() != WL_CONNECTED) {
    code = StatusLedCode::WifiConnecting;
  } else if (!environmentSensorPresent() && !radarSensorPresent()) {
    code = StatusLedCode::NoSensor;
  } else if (environmentReadFault) {
    code = StatusLedCode::EnvironmentError;
  } else if (radarSensorPresent() && !radarSensorReading().online) {
    code = StatusLedCode::RadarError;
  } else if (presenceLightIsOn()) {
    code = StatusLedCode::UserLight;
  } else if (!pogdevIsConnected()) {
    if (pogdevIsAdopted()) {
      if (!pogHomeDisconnectedSince) pogHomeDisconnectedSince = now;
      code = now - pogHomeDisconnectedSince >= 60000
                 ? StatusLedCode::PogHomeError
                 : StatusLedCode::PogHomeWaiting;
    } else {
      pogHomeDisconnectedSince = 0;
      code = StatusLedCode::PogHomeWaiting;
    }
  } else {
    pogHomeDisconnectedSince = 0;
    code = ota == OtaVisualState::Available ? StatusLedCode::OtaAvailable
                                            : StatusLedCode::Ready;
  }

  statusLedSetUserLight(presenceLightColor(), presenceLightBrightness());
  statusLedSet(code);
  statusLedLoop(now);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.printf("\n=== POG Sensor %s ===\n", POGSENSOR_FW_VERSION);
  Serial.printf("[Firmware] %s\n", kFirmwareIdentity);
  configBegin();
  configLoad();
  statusLedBegin(g_config.statusLightInstalled);
  bool sensorFound = environmentSensorBegin();
  Serial.printf("[Capteur] %s, SDA=%u SCL=%u%s\n",
                environmentSensorModel(), g_config.sdaPin, g_config.sclPin,
                sensorFound ? "" : " (vérifiez le câblage)");
  radarSensorBegin();
  presenceLightBegin();
  radarWasPresent = radarSensorPresent();
  if (radarWasPresent) {
    Serial.printf("[Radar] %s, ports RX/TX %u/%u et %u/%u\n",
                  radarSensorModel(), g_config.radarARxPin,
                  g_config.radarATxPin, g_config.radarBRxPin,
                  g_config.radarBTxPin);
    pogdevSetRadar(radarSensorReading(), true);
  }
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
  environmentSensorLoop();
  sampleEnvironment(now);
  sampleRadar();
  if (presenceLightUpdate(radarSensorReading().online,
                          radarSensorReading().occupied, now)) {
    pogdevRefreshState();
  }
  pogdevLoop();
  updateStatusLed(now);
  delay(5);
}
