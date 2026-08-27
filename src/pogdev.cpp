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
#include "pogdev_retry.h"
#include "presence_light.h"
#include "radar_sensor.h"

#ifndef POGSENSOR_FW_VERSION
#define POGSENSOR_FW_VERSION "dev"
#endif

namespace {
constexpr char kNamespace[] = "pogdev";
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
RadarReading currentRadar;
bool hasRadarReading = false;
bool manifestDirty = false;
pogdev::Backoff mqttBackoff;
pogdev::AuthGate authGate;
pogdev::OfflineWatch offlineWatch;

String deviceModel() {
  String result = String("POG Sensor · ") + environmentSensorModel();
  if (radarSensorPresent()) result += String(" + ") + radarSensorModel();
  return result;
}

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
    request["model"] = deviceModel();
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

void addReadOnlyTraitEntity(JsonArray entities, const char *key,
                            const char *name, const char *traitId,
                            const char *category) {
  JsonObject entity = entities.add<JsonObject>();
  entity["key"] = key;
  entity["name"] = name;
  entity["category"] = category;
  JsonObject trait = entity["traits"].to<JsonArray>().add<JsonObject>();
  trait["id"] = traitId;
  trait["config"]["read_only"] = true;
}

void addBinary(JsonArray entities, const char *key, const char *name,
               const char *kind, const char *category = "presence") {
  JsonObject entity = entities.add<JsonObject>();
  entity["key"] = key;
  entity["name"] = name;
  entity["category"] = category;
  JsonObject trait = entity["traits"].to<JsonArray>().add<JsonObject>();
  trait["id"] = "binary";
  trait["config"]["kind"] = kind;
  trait["config"]["read_only"] = true;
}

void publishHello() {
  JsonDocument doc;
  doc["proto"] = 1;
  doc["hw_id"] = hardwareId;
  doc["model"] = deviceModel();
  doc["fw_version"] = POGSENSOR_FW_VERSION;
  doc["name"] = g_config.name;
  JsonArray entities = doc["entities"].to<JsonArray>();
  if (environmentHasTemperature()) {
    addMeasurement(entities, "temperature", "Température", "temperature", "°C");
  }
  if (environmentHasHumidity()) {
    addMeasurement(entities, "humidity", "Humidité", "humidity", "%");
  }
  if (environmentHasPressure()) {
    addMeasurement(entities, "pressure", "Pression", "pressure", "hPa");
  }
  if (environmentHasCo2()) {
    addMeasurement(entities, "co2", "CO₂", "co2", "ppm", "air_quality");
  }
  if (environmentHasIlluminance()) {
    addMeasurement(entities, "illuminance", "Luminosité", "illuminance", "lx",
                   "environment");
  }
  if (environmentHasVocIndex()) {
    addMeasurement(entities, "voc_index", "Indice COV", "voc_index", "",
                   "air_quality");
  }
  if (environmentHasGasResistance()) {
    addMeasurement(entities, "gas_resistance", "Résistance gaz",
                   "gas_resistance", "kΩ", "diagnostic");
  }
  if (radarSensorPresent()) {
    addReadOnlyTraitEntity(entities, "presence", "Présence", "presence",
                           "presence");
    addBinary(entities, "motion", "Mouvement", "motion");
    addReadOnlyTraitEntity(entities, "radar_tracking", "Suivi spatial",
                           "radar_tracking", "presence");
  }

  if (g_config.statusLightInstalled) {
    JsonObject light = entities.add<JsonObject>();
    light["key"] = "presence_light";
    light["name"] = "Lumière de présence";
    light["category"] = "light";
    JsonArray lightTraits = light["traits"].to<JsonArray>();
    JsonObject lightPower = lightTraits.add<JsonObject>();
    lightPower["id"] = "on_off";
    lightPower["config"]["purpose"] = "nightlight";
    lightPower["config"]["purpose_label"] = "Veilleuse";
    lightTraits.add<JsonObject>()["id"] = "brightness";
    lightTraits.add<JsonObject>()["id"] = "color";

    JsonObject automatic = entities.add<JsonObject>();
    automatic["key"] = "presence_light_auto";
    automatic["name"] = "Allumage sur présence";
    automatic["category"] = "light";
    automatic["traits"].to<JsonArray>().add<JsonObject>()["id"] = "on_off";

    JsonObject hold = entities.add<JsonObject>();
    hold["key"] = "presence_light_hold";
    hold["name"] = "Maintien après présence";
    hold["category"] = "light";
    JsonObject holdTrait = hold["traits"].to<JsonArray>().add<JsonObject>();
    holdTrait["id"] = "number";
    holdTrait["config"]["min"] = 0;
    holdTrait["config"]["max"] = 300;
    holdTrait["config"]["step"] = 1;
    holdTrait["config"]["unit"] = "s";
  }
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
  if (currentReading.hasTemperature) {
    doc["temperature"]["value"] =
        roundf(currentReading.temperature * 10.0f) / 10.0f;
    doc["temperature"]["kind"] = "temperature";
  }
  if (currentReading.hasHumidity) {
    doc["humidity"]["value"] = roundf(currentReading.humidity * 10.0f) / 10.0f;
    doc["humidity"]["kind"] = "humidity";
  }
  if (currentReading.hasPressure) {
    doc["pressure"]["value"] =
        roundf(currentReading.pressure * 10.0f) / 10.0f;
    doc["pressure"]["kind"] = "pressure";
  }
  if (currentReading.hasCo2) {
    doc["co2"]["value"] = roundf(currentReading.co2);
    doc["co2"]["kind"] = "co2";
  }
  if (currentReading.hasIlluminance) {
    doc["illuminance"]["value"] =
        roundf(currentReading.illuminance * 10.0f) / 10.0f;
    doc["illuminance"]["kind"] = "illuminance";
  }
  if (currentReading.hasVocIndex) {
    doc["voc_index"]["value"] = roundf(currentReading.vocIndex);
    doc["voc_index"]["kind"] = "voc_index";
  }
  if (currentReading.hasGasResistance) {
    doc["gas_resistance"]["value"] =
        roundf(currentReading.gasResistance * 10.0f) / 10.0f;
    doc["gas_resistance"]["kind"] = "gas_resistance";
  }
  if (hasRadarReading && radarSensorPresent()) {
    doc["presence"]["occupied"] = currentRadar.occupied;
    doc["presence"]["probability"] = currentRadar.occupied ? 1.0 : 0.0;
    doc["motion"]["active"] = currentRadar.motion;
    doc["motion"]["kind"] = "motion";
    JsonObject tracking = doc["radar_tracking"].to<JsonObject>();
    tracking["occupied"] = currentRadar.occupied;
    tracking["target_count"] = currentRadar.targetCount;
    tracking["moving_distance_m"] = currentRadar.movingDistanceCm / 100.0f;
    tracking["stationary_distance_m"] =
        currentRadar.stationaryDistanceCm / 100.0f;
    tracking["nearest_distance_m"] =
        currentRadar.detectionDistanceCm / 100.0f;
    for (size_t i = 0; i < 3; ++i) {
      const RadarTarget &target = currentRadar.targets[i];
      String prefix = "target_" + String(i + 1) + "_";
      tracking[prefix + "active"] = target.active;
      if (!target.active) continue;
      tracking[prefix + "x_m"] = target.xMm / 1000.0f;
      tracking[prefix + "y_m"] = target.yMm / 1000.0f;
      tracking[prefix + "speed_m_s"] = target.speedCmS / 100.0f;
      tracking[prefix + "resolution_m"] = target.resolutionMm / 1000.0f;
    }
  }
  if (g_config.statusLightInstalled) {
    presenceLightFillState(doc["presence_light"].to<JsonObject>(),
                           doc["presence_light_auto"].to<JsonObject>(),
                           doc["presence_light_hold"].to<JsonObject>());
  }
  doc["wifi_signal"]["value"] = currentReading.wifiRssi;
  doc["wifi_signal"]["kind"] = "signal_strength";
  doc["sensor_status"]["active"] =
      currentReading.sensorOnline || (hasRadarReading && currentRadar.online);
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
  String key = doc["key"].as<String>();
  String name = doc["name"].as<String>();
  JsonObjectConst params = doc["params"].as<JsonObjectConst>();
  bool changed = false;
  bool persist = false;

  if (!g_config.statusLightInstalled && key.startsWith("presence_light")) {
    Serial.println("[PogHome] commande LED refusée: module non installé");
    return;
  }
  if (key == "presence_light") {
    if (name == "turn_on") {
      presenceLightTurnOn();
      changed = true;
    } else if (name == "turn_off") {
      presenceLightTurnOff();
      changed = true;
    } else if (name == "toggle") {
      presenceLightToggle();
      changed = true;
    } else if (name == "set_brightness") {
      presenceLightSetBrightness(params["brightness"] | 0.0f);
      changed = persist = true;
    } else if (name == "set_hs") {
      presenceLightSetHs(params["hue"] | 0.0f,
                         params["saturation"] | 0.0f);
      changed = persist = true;
    } else if (name == "set_color_temp") {
      presenceLightSetColorTemperature(params["kelvin"] | 2700.0f);
      changed = persist = true;
    }
  } else if (key == "presence_light_auto") {
    bool enabled = presenceLightAutomatic();
    if (name == "turn_on") enabled = true;
    else if (name == "turn_off") enabled = false;
    else if (name == "toggle") enabled = !enabled;
    else enabled = presenceLightAutomatic();
    if (enabled != presenceLightAutomatic()) {
      presenceLightSetAutomatic(enabled);
      changed = persist = true;
    }
  } else if (key == "presence_light_hold" && name == "set_value") {
    presenceLightSetHoldSeconds(params["value"] | 8.0f);
    changed = persist = true;
  }

  if (!changed) {
    Serial.printf("[PogHome] commande ignorée: %s/%s\n", key.c_str(),
                  name.c_str());
    return;
  }
  if (persist && !configSave()) {
    Serial.println("[LED] impossible d’enregistrer le réglage");
  }
  stateDirty = true;
  Serial.printf("[PogHome] lumière: %s/%s\n", key.c_str(), name.c_str());
}

bool connectMqtt() {
  mqtt.setServer(credentials.host.c_str(), credentials.port);
  mqtt.setCallback(handleCommand);
  mqtt.setBufferSize(4096);
  mqtt.setKeepAlive(30);
  String statusTopic = "pog/" + credentials.deviceId + "/status";
  bool ok = mqtt.connect(credentials.deviceId.c_str(),
                         credentials.deviceId.c_str(),
                         credentials.password.c_str(), statusTopic.c_str(), 1,
                         true, "offline", true);
  if (!ok) {
    if (mqtt.state() == MQTT_CONNECT_UNAUTHORIZED ||
        mqtt.state() == MQTT_CONNECT_BAD_CREDENTIALS) {
      // Un CONNACK refusé n'est plus un ordre d'oubli : pendant que POG Home
      // redémarre, le courtier refuse avec des comptes pas encore
      // reprovisionnés, et l'effacement coûtait l'adoption entière du foyer —
      // irrécupérable sans humain (panne du 27 août 2026). La relève tranche à
      // sa place : 404 = vraiment oublié (announceAndCollect efface),
      // « pending » = demande de ré-adoption posée dans l'inventaire,
      // « adopted » = identifiants neufs enregistrés.
      if (pogdev::authGateRejected(authGate, millis()) &&
          (poghomeAddress || discoverPogHome())) {
        Serial.println("[PogHome] identifiants refusés avec insistance : "
                       "relève auprès de POG Home");
        announceAndCollect(false);
      }
    }
    return false;
  }
  pogdev::authGateConnected(authGate);
  pogdev::backoffConnected(mqttBackoff);
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
  bool wifiUp = WiFi.status() == WL_CONNECTED;
  // Le filet du 27 août : le remède constaté était de débrancher puis
  // rebrancher chaque appareil. Trente minutes continues adopté, Wi-Fi debout
  // et courtier absent, et on refait ce geste tout seul — ça guérit aussi ce
  // que le code ne sait pas énumérer (sockets épuisées, pile figée).
  if (pogdev::offlineWatchTick(
          offlineWatch, wifiUp && credentials.valid() && !mqtt.connected(),
          now)) {
    Serial.println("[PogHome] 30 min sans courtier malgré le Wi-Fi : "
                   "redémarrage");
    delay(80);
    ESP.restart();
  }
  if (!wifiUp) {
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
      if (connectMqtt()) {
        nextReconnect = now;
      } else {
        if (credentials.valid() && (int32_t)(now - nextRediscovery) >= 0) {
          poghomeAddress = IPAddress();
          if (discoverPogHome()) {
            credentials.host = poghomeAddress.toString();
            saveCredentials(credentials);
          }
          nextRediscovery = now + 30000;
        }
        // Reprise sans fin : 5 s doublées jusqu'à 60 s. Le pas ne revient à la
        // base qu'au CONNACK accepté (backoffConnected), jamais avant — un
        // appareil encastré doit retrouver un courtier qui revient des heures
        // plus tard sans le marteler quand il est absent.
        nextReconnect = now + pogdev::backoffNextDelayMs(mqttBackoff);
      }
    }
    return;
  }
  mqtt.loop();
  if (manifestDirty) {
    publishHello();
    manifestDirty = false;
  }
  if (stateDirty || (int32_t)(now - nextFullState) >= 0) publishState();
}

void pogdevSetReading(const pogsensor::Reading &reading, bool forcePublish) {
  currentReading = reading;
  hasReading = true;
  stateDirty = stateDirty || forcePublish;
}

void pogdevSetRadar(const RadarReading &reading, bool forcePublish) {
  currentRadar = reading;
  hasRadarReading = true;
  stateDirty = stateDirty || forcePublish;
}

void pogdevRefreshManifest() { manifestDirty = true; }
void pogdevRefreshState() { stateDirty = true; }

const String &pogdevHardwareId() { return hardwareId; }
bool pogdevIsAdopted() { return credentials.valid(); }
bool pogdevIsConnected() { return mqtt.connected(); }
