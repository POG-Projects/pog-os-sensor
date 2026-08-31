#include "config.h"

#include <ArduinoJson.h>
#include <Preferences.h>

namespace {
constexpr char kNamespace[] = "pogsensor";
constexpr char kConfigKey[] = "config";
}

DeviceConfig g_config;

void configBegin() {}

bool configLoad() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;
  String json = prefs.getString(kConfigKey, "");
  prefs.end();
  if (!json.length()) return false;

  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  g_config.wifiSsid = doc["wifi_ssid"] | "";
  g_config.wifiPassword = doc["wifi_password"] | "";
  g_config.name = doc["name"] | "Capteur POG";
  g_config.pogHomeHost = doc["poghome_host"] | "";
  g_config.pogHomePort =
      constrain(doc["poghome_port"] | 8090, 1, 65535);
  g_config.sdaPin = constrain(doc["sda_pin"] | POGSENSOR_DEFAULT_SDA, 0, 48);
  g_config.sclPin = constrain(doc["scl_pin"] | POGSENSOR_DEFAULT_SCL, 0, 48);
  g_config.radarARxPin =
      constrain(doc["radar_a_rx"] | POGSENSOR_RADAR_A_RX, 0, 48);
  g_config.radarATxPin =
      constrain(doc["radar_a_tx"] | POGSENSOR_RADAR_A_TX, 0, 48);
  g_config.radarBRxPin =
      constrain(doc["radar_b_rx"] | POGSENSOR_RADAR_B_RX, 0, 48);
  g_config.radarBTxPin =
      constrain(doc["radar_b_tx"] | POGSENSOR_RADAR_B_TX, 0, 48);
  g_config.statusLightInstalled = doc["status_light_installed"] | false;
  g_config.presenceLightAuto = doc["presence_light_auto"] | true;
  g_config.presenceLightBrightness = constrain(
      doc["presence_light_brightness"] | 55, 0, 100);
  g_config.presenceLightColor =
      doc["presence_light_color"] | static_cast<uint32_t>(0xFFD28A);
  g_config.presenceLightColor &= 0xFFFFFF;
  g_config.presenceLightColorTemperature =
      doc["presence_light_color_temperature"] | false;
  g_config.presenceLightKelvin = constrain(
      doc["presence_light_kelvin"] | 2700, 1000, 10000);
  g_config.presenceLightHoldSeconds = constrain(
      doc["presence_light_hold"] | 8, 0, 300);
  g_config.samplePeriodSeconds =
      constrain(doc["sample_period"] | 30, 5, 3600);
  g_config.temperatureOffset =
      constrain(doc["temperature_offset"] | 0.0f, -10.0f, 10.0f);
  if (!g_config.name.length()) g_config.name = "Capteur POG";
  return true;
}

bool configSave() {
  JsonDocument doc;
  doc["wifi_ssid"] = g_config.wifiSsid;
  doc["wifi_password"] = g_config.wifiPassword;
  doc["name"] = g_config.name;
  doc["poghome_host"] = g_config.pogHomeHost;
  doc["poghome_port"] = g_config.pogHomePort;
  doc["sda_pin"] = g_config.sdaPin;
  doc["scl_pin"] = g_config.sclPin;
  doc["radar_a_rx"] = g_config.radarARxPin;
  doc["radar_a_tx"] = g_config.radarATxPin;
  doc["radar_b_rx"] = g_config.radarBRxPin;
  doc["radar_b_tx"] = g_config.radarBTxPin;
  doc["status_light_installed"] = g_config.statusLightInstalled;
  doc["presence_light_auto"] = g_config.presenceLightAuto;
  doc["presence_light_brightness"] = g_config.presenceLightBrightness;
  doc["presence_light_color"] = g_config.presenceLightColor;
  doc["presence_light_color_temperature"] =
      g_config.presenceLightColorTemperature;
  doc["presence_light_kelvin"] = g_config.presenceLightKelvin;
  doc["presence_light_hold"] = g_config.presenceLightHoldSeconds;
  doc["sample_period"] = g_config.samplePeriodSeconds;
  doc["temperature_offset"] = g_config.temperatureOffset;
  String json;
  serializeJson(doc, json);

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  bool ok = prefs.putString(kConfigKey, json) == json.length();
  prefs.end();
  return ok;
}
