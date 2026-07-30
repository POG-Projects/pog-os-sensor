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
