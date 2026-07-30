#include "pogdev.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_system.h>

#include "config.h"
#include "environment_sensor.h"

#ifndef POGSENSOR_FW_VERSION
#define POGSENSOR_FW_VERSION "dev"
#endif

namespace {
constexpr char kNamespace[] = "pogdev";
constexpr uint32_t kReconnectPeriodMs = 5000;
constexpr uint32_t kFullStatePeriodMs = 300000;

struct Credentials {
  String deviceId;
  String host;
  uint16_t port = 1883;
  String password;

  bool valid() const {
    return deviceId.length() && host.length() && password.length();
  }
};

WiFiClient mqttTransport;
PubSubClient mqtt(mqttTransport);
Credentials credentials;
String claimSecret;
String hardwareId;
IPAddress poghomeAddress;
uint16_t poghomeApiPort = 8090;
pogsensor::Reading currentReading;
bool hasReading = false;
bool stateDirty = false;
uint32_t nextEnrolment = 0;
uint32_t nextReconnect = 0;
uint32_t nextFullState = 0;
uint32_t enrolmentStarted = 0;
uint32_t nextRediscovery = 0;
bool refreshAnnouncement = true;

String makeHardwareId() {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_BASE);
  char out[40];
  snprintf(out, sizeof(out), "ESP-SENSOR-%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(out);
}

String makeSecret() {
  char out[65];
  for (size_t i = 0; i < 32; ++i) {
    snprintf(out + i * 2, 3, "%02x", esp_random() & 0xff);
  }
  out[64] = '\0';
  return String(out);
}

void loadIdentity() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  claimSecret = prefs.getString("claim", "");
  credentials.deviceId = prefs.getString("dev_id", "");
  credentials.host = prefs.getString("host", "");
  credentials.port = prefs.getUShort("port", 1883);
  credentials.password = prefs.getString("mqtt_pw", "");
  prefs.end();

  if (!claimSecret.length()) {
    claimSecret = makeSecret();
    prefs.begin(kNamespace, false);
    prefs.putString("claim", claimSecret);
    prefs.end();
  }
}

bool saveCredentials(const Credentials &next) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  bool ok = prefs.putString("dev_id", next.deviceId) == next.deviceId.length() &&
            prefs.putString("host", next.host) == next.host.length() &&
            prefs.putUShort("port", next.port) == sizeof(uint16_t) &&
            prefs.putString("mqtt_pw", next.password) == next.password.length();
  prefs.end();
  if (!ok) return false;

  Credentials verified;
  prefs.begin(kNamespace, true);
  verified.deviceId = prefs.getString("dev_id", "");
  verified.host = prefs.getString("host", "");
  verified.port = prefs.getUShort("port", 0);
  verified.password = prefs.getString("mqtt_pw", "");
  prefs.end();
  if (!verified.valid() || verified.deviceId != next.deviceId ||
      verified.password != next.password) {
    return false;
  }
  credentials = verified;
  return true;
}

void clearIdentity() {
  mqtt.disconnect();
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.clear();
  prefs.end();
  credentials = Credentials{};
  claimSecret = makeSecret();
  prefs.begin(kNamespace, false);
  prefs.putString("claim", claimSecret);
  prefs.end();
  enrolmentStarted = millis();
  nextEnrolment = 0;
}

bool resolveConfiguredPogHome() {
  if (!g_config.pogHomeHost.length()) return false;
  if (!WiFi.hostByName(g_config.pogHomeHost.c_str(), poghomeAddress)) return false;
  poghomeApiPort = g_config.pogHomePort;
  return true;
}

bool discoverPogHome() {
  int count = MDNS.queryService("poghome", "tcp");
  for (int i = 0; i < count; ++i) {
    String proto = MDNS.txt(i, "proto");
    if (proto.length() && proto != "1") continue;
    IPAddress address = MDNS.address(i);
    if (!address) continue;
    poghomeAddress = address;
    String api = MDNS.txt(i, "api");
    poghomeApiPort = api.length() ? api.toInt() : MDNS.port(i);
    if (!poghomeApiPort) poghomeApiPort = 8090;
    Serial.printf("[PogHome] détecté sur %s:%u\n",
                  poghomeAddress.toString().c_str(), poghomeApiPort);
    return true;
  }
  return resolveConfiguredPogHome();
}

String apiUrl(const String &path) {
  return "http://" + poghomeAddress.toString() + ":" +
         String(poghomeApiPort) + path;
}

int httpPostJson(const String &path, const String &body, String &response) {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, apiUrl(path))) return -1;
  http.setConnectTimeout(3000);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  response = http.getString();
  http.end();
  return code;
}

int httpGet(const String &path, String &response) {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, apiUrl(path))) return -1;
  http.setConnectTimeout(3000);
  http.setTimeout(5000);
  int code = http.GET();
  response = http.getString();
  http.end();
  return code;
}

bool announceAndCollect(bool announce) {
  String response;
  int code = 0;
  if (announce) {
    JsonDocument request;
    request["hw_id"] = hardwareId;
    request["model"] = String("POG Sensor · ") + environmentSensorModel();
    request["fw_version"] = POGSENSOR_FW_VERSION;
    request["proto_version"] = "1";
    request["name"] = g_config.name;
    request["claim_secret"] = claimSecret;
    String body;
    serializeJson(request, body);
    code = httpPostJson("/api/v1/pogdev/announce", body, response);
    if (code != 202 && code != 200 && code != 409) return false;
  }

  String path = "/api/v1/pogdev/announce/" + hardwareId +
                "?secret=" + claimSecret;
  code = httpGet(path, response);
  if (code == 404 || code == 410) {
    clearIdentity();
    return false;
  }
  if (code != 200) return false;

  JsonDocument doc;
  if (deserializeJson(doc, response) || doc["status"] != "adopted") return false;
  Credentials next;
  next.deviceId = doc["device_id"].as<String>();
  next.host = doc["mqtt"]["host"].as<String>();
  next.port = doc["mqtt"]["port"] | 1883;
  next.password = doc["mqtt"]["password"].as<String>();
  if (!next.host.length()) next.host = poghomeAddress.toString();
  if (!next.valid() || !saveCredentials(next)) return false;
  Serial.printf("[PogHome] adopté comme %s\n", credentials.deviceId.c_str());
  return true;
}

void addMeasurement(JsonArray entities, const char *key, const char *name,
                    const char *kind, const char *unit,
                    const char *category = "climate") {
  JsonObject entity = entities.add<JsonObject>();
  entity["key"] = key;
  entity["name"] = name;
  entity["category"] = category;
  JsonObject trait = entity["traits"].to<JsonArray>().add<JsonObject>();
  trait["id"] = "measurement";
  trait["config"]["kind"] = kind;
  trait["config"]["unit"] = unit;
  trait["config"]["read_only"] = true;
}

void publishHello() {
  JsonDocument doc;
  doc["proto"] = 1;
  doc["hw_id"] = hardwareId;
  doc["model"] = String("POG Sensor · ") + environmentSensorModel();
  doc["fw_version"] = POGSENSOR_FW_VERSION;
  doc["name"] = g_config.name;
  JsonArray entities = doc["entities"].to<JsonArray>();
  addMeasurement(entities, "temperature", "Température", "temperature", "°C");
  if (environmentSensorKind() == SensorKind::Bme280) {
    addMeasurement(entities, "humidity", "Humidité", "humidity", "%");
  }
  addMeasurement(entities, "pressure", "Pression", "pressure", "hPa");
  addMeasurement(entities, "wifi_signal", "Signal Wi-Fi", "signal_strength",
                 "dBm", "diagnostic");

  JsonObject status = entities.add<JsonObject>();
  status["key"] = "sensor_status";
  status["name"] = "État du capteur";
  status["category"] = "diagnostic";
  JsonObject statusTrait =
      status["traits"].to<JsonArray>().add<JsonObject>();
  statusTrait["id"] = "binary";
  statusTrait["config"]["kind"] = "connectivity";
  statusTrait["config"]["read_only"] = true;
  doc["local_rules"].to<JsonArray>();

  String payload;
  serializeJson(doc, payload);
  String topic = "pog/" + credentials.deviceId + "/hello";
  bool ok = !doc.overflowed() &&
            mqtt.publish(topic.c_str(), payload.c_str(), true);
  Serial.printf("[PogHome] manifeste %s (%u octets)\n",
                ok ? "publié" : "en échec", payload.length());
}

void publishState() {
  if (!hasReading) return;
  JsonDocument doc;
  doc["temperature"]["value"] = roundf(currentReading.temperature * 10.0f) / 10.0f;
  doc["temperature"]["kind"] = "temperature";
  if (currentReading.hasHumidity) {
    doc["humidity"]["value"] = roundf(currentReading.humidity * 10.0f) / 10.0f;
    doc["humidity"]["kind"] = "humidity";
  }
  doc["pressure"]["value"] = roundf(currentReading.pressure * 10.0f) / 10.0f;
  doc["pressure"]["kind"] = "pressure";
  doc["wifi_signal"]["value"] = currentReading.wifiRssi;
  doc["wifi_signal"]["kind"] = "signal_strength";
  doc["sensor_status"]["active"] = currentReading.sensorOnline;
  doc["sensor_status"]["kind"] = "connectivity";
  String payload;
  serializeJson(doc, payload);
  String topic = "pog/" + credentials.deviceId + "/state";
  if (mqtt.publish(topic.c_str(), payload.c_str(), true)) {
    stateDirty = false;
    nextFullState = millis() + kFullStatePeriodMs;
  }
}

void handleCommand(char *, byte *payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  Serial.printf("[PogHome] commande ignorée (capteur lecture seule): %s/%s\n",
                doc["key"] | "?", doc["name"] | "?");
}

bool connectMqtt() {
  mqtt.setServer(credentials.host.c_str(), credentials.port);
  mqtt.setCallback(handleCommand);
  mqtt.setBufferSize(2048);
  mqtt.setKeepAlive(30);
  String statusTopic = "pog/" + credentials.deviceId + "/status";
  bool ok = mqtt.connect(credentials.deviceId.c_str(),
                         credentials.deviceId.c_str(),
                         credentials.password.c_str(), statusTopic.c_str(), 1,
                         true, "offline", true);
  if (!ok) {
    if (mqtt.state() == MQTT_CONNECT_UNAUTHORIZED ||
        mqtt.state() == MQTT_CONNECT_BAD_CREDENTIALS) {
      Serial.println("[PogHome] compte supprimé, nouvelle adoption");
      clearIdentity();
    }
    return false;
  }
  String cmdTopic = "pog/" + credentials.deviceId + "/cmd";
  mqtt.subscribe(cmdTopic.c_str(), 1);
  mqtt.publish(statusTopic.c_str(), "online", true);
  publishHello();
  publishState();
  Serial.println("[PogHome] MQTT connecté");
  return true;
}
}  // namespace

void pogdevBegin() {
  hardwareId = makeHardwareId();
  loadIdentity();
  enrolmentStarted = millis();
}

void pogdevLoop() {
  uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    mqtt.disconnect();
    return;
  }
  if (!credentials.valid()) {
    if ((int32_t)(now - nextEnrolment) < 0) return;
    if ((poghomeAddress || discoverPogHome()) &&
        announceAndCollect(refreshAnnouncement)) {
      nextReconnect = 0;
    } else {
      poghomeAddress = IPAddress();
    }
    refreshAnnouncement = !refreshAnnouncement;
    uint32_t elapsed = now - enrolmentStarted;
    uint32_t interval = elapsed < 60000 ? 5000 :
                        elapsed < 3600000 ? 30000 : 300000;
    nextEnrolment = now + interval;
    return;
  }
  if (!mqtt.connected()) {
    if ((int32_t)(now - nextReconnect) >= 0) {
      if (!connectMqtt() && credentials.valid() &&
          (int32_t)(now - nextRediscovery) >= 0) {
        poghomeAddress = IPAddress();
        if (discoverPogHome()) {
          credentials.host = poghomeAddress.toString();
          saveCredentials(credentials);
        }
        nextRediscovery = now + 30000;
      }
      nextReconnect = now + kReconnectPeriodMs;
    }
    return;
  }
  mqtt.loop();
  if (stateDirty || (int32_t)(now - nextFullState) >= 0) publishState();
}

void pogdevSetReading(const pogsensor::Reading &reading, bool forcePublish) {
  currentReading = reading;
  hasReading = true;
  stateDirty = stateDirty || forcePublish;
}

const String &pogdevHardwareId() { return hardwareId; }
bool pogdevIsAdopted() { return credentials.valid(); }
bool pogdevIsConnected() { return mqtt.connected(); }
