#include "web_portal.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ctype.h>
#include <stdlib.h>

#include "config.h"
#include "environment_sensor.h"
#include "ota_update.h"
#include "pogdev.h"
#include "presence_light.h"
#include "radar_sensor.h"
#include "status_led.h"
#include "web_auth.h"
#include "web_ui.h"

namespace {
WebServer server(80);
DNSServer dns;
bool captive = false;
bool otaUploadAuthorized = false;
uint8_t loginFailures = 0;
uint32_t loginLockoutUntil = 0;

void sendError(int code, const char *message);

bool requestAuthorized() {
  return !webAuthHasPassword() ||
         webAuthTokenValid(server.header("X-Auth-Token"));
}

bool requireAuth() {
  if (requestAuthorized()) return true;
  server.sendHeader("Cache-Control", "no-store");
  server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  return false;
}

void handleAuthStatus() {
  JsonDocument doc;
  doc["hasPassword"] = webAuthHasPassword();
  doc["authed"] = requestAuthorized();
  String payload;
  serializeJson(doc, payload);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", payload);
}

void handleAuthSetup() {
  if (webAuthHasPassword()) {
    server.send(403, "application/json",
                "{\"success\":false,\"error\":\"already_set\"}");
    return;
  }
  String body = server.arg("plain");
  if (body.length() > 256) {
    sendError(413, "La requête est trop volumineuse.");
    return;
  }
  JsonDocument request;
  if (deserializeJson(request, body)) {
    sendError(400, "La requête est invalide.");
    return;
  }
  String password = request["password"].as<String>();
  if (!webAuthSetPassword(password)) {
    sendError(400, "Le mot de passe doit contenir entre 8 et 128 caractères.");
    return;
  }
  JsonDocument response;
  response["success"] = true;
  response["token"] = webAuthIssueToken();
  String payload;
  serializeJson(response, payload);
  server.send(200, "application/json", payload);
}

void handleAuthLogin() {
  uint32_t now = millis();
  if (!webAuthHasPassword()) {
    server.send(400, "application/json",
                "{\"success\":false,\"error\":\"no_password\"}");
    return;
  }
  if (loginLockoutUntil && (int32_t)(now - loginLockoutUntil) < 0) {
    server.send(429, "application/json",
                "{\"success\":false,\"error\":\"locked_out\"}");
    return;
  }
  String body = server.arg("plain");
  if (body.length() > 256) {
    sendError(413, "La requête est trop volumineuse.");
    return;
  }
  JsonDocument request;
  if (deserializeJson(request, body)) {
    sendError(400, "La requête est invalide.");
    return;
  }
  String password = request["password"].as<String>();
  if (!webAuthCheckPassword(password)) {
    delay(300);
    if (++loginFailures >= 5) {
      loginFailures = 0;
      loginLockoutUntil = millis() + 30000;
    }
    server.send(401, "application/json", "{\"success\":false}");
    return;
  }
  loginFailures = 0;
  loginLockoutUntil = 0;
  JsonDocument response;
  response["success"] = true;
  response["token"] = webAuthIssueToken();
  String payload;
  serializeJson(response, payload);
  server.send(200, "application/json", payload);
}

void handleAuthPassword() {
  if (!requireAuth()) return;
  String body = server.arg("plain");
  if (body.length() > 384) {
    sendError(413, "La requête est trop volumineuse.");
    return;
  }
  JsonDocument request;
  if (deserializeJson(request, body)) {
    sendError(400, "La requête est invalide.");
    return;
  }
  String currentPassword = request["current_password"].as<String>();
  String newPassword = request["new_password"].as<String>();
  if (!webAuthCheckPassword(currentPassword)) {
    delay(300);
    sendError(403, "Le mot de passe actuel est incorrect.");
    return;
  }
  if (!webAuthSetPassword(newPassword)) {
    sendError(400, "Le nouveau mot de passe doit contenir entre 8 et 128 caractères.");
    return;
  }
  webAuthInvalidateTokens();
  JsonDocument response;
  response["success"] = true;
  response["token"] = webAuthIssueToken();
  String payload;
  serializeJson(response, payload);
  server.send(200, "application/json", payload);
}

void handleAuthLogout() {
  if (!requireAuth()) return;
  webAuthRevokeToken(server.header("X-Auth-Token"));
  server.send(200, "application/json", "{\"success\":true}");
}

void sendSetupPage() {
  server.send_P(200, "text/html; charset=utf-8", kSetupPage);
}

void redirectToPortal() {
  if (captive) {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  } else {
    sendSetupPage();
  }
}

void sendStatus() {
  if (!requireAuth()) return;
  JsonDocument doc;
  doc["sensor"] = environmentSensorModel();
  doc["sensor_online"] = environmentSensorPresent() || radarSensorPresent();
  doc["address"] = environmentSensorAddresses();
  doc["radar"] = radarSensorModel();
  doc["radar_a_wiring"] = radarSensorWiringName(radarSensorPortWiring(0));
  doc["radar_b_wiring"] = radarSensorWiringName(radarSensorPortWiring(1));
  doc["hw_id"] = pogdevHardwareId();
  doc["poghome_status"] = pogdevIsConnected() ? "connecté à POG Home" :
                              pogdevIsAdopted() ? "adopté" :
                                                "en attente d’adoption";
  doc["ssid"] = g_config.wifiSsid;
  doc["name"] = g_config.name;
  doc["poghome"] = g_config.pogHomeHost;
  doc["poghome_port"] = g_config.pogHomePort;
  doc["sda"] = g_config.sdaPin;
  doc["scl"] = g_config.sclPin;
  doc["radar_a_rx"] = g_config.radarARxPin;
  doc["radar_a_tx"] = g_config.radarATxPin;
  doc["radar_b_rx"] = g_config.radarBRxPin;
  doc["radar_b_tx"] = g_config.radarBTxPin;
  doc["status_light_installed"] = g_config.statusLightInstalled;
  doc["status_light_pin"] = POGSENSOR_STATUS_LED_PIN;
  doc["status_light_count"] = POGSENSOR_STATUS_LED_COUNT;
  doc["presence_light_auto"] = g_config.presenceLightAuto;
  doc["presence_light_brightness"] = g_config.presenceLightBrightness;
  char presenceColor[8];
  snprintf(presenceColor, sizeof(presenceColor), "#%06lX",
           static_cast<unsigned long>(g_config.presenceLightColor));
  doc["presence_light_color"] = presenceColor;
  doc["presence_light_hold"] = g_config.presenceLightHoldSeconds;
  if (g_config.statusLightInstalled) {
    doc["status_led"] = statusLedCodeName(statusLedCode());
    doc["presence_light_on"] = presenceLightIsOn();
  }
  doc["period"] = g_config.samplePeriodSeconds;
  doc["offset"] = g_config.temperatureOffset;
  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void sendNetworks() {
  if (!requireAuth()) return;
  int result = WiFi.scanComplete();
  if (result == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true, true);
    server.send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  if (result == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  JsonDocument doc;
  doc["scanning"] = false;
  JsonArray networks = doc["networks"].to<JsonArray>();
  // Le pilote renvoie les réseaux par RSSI décroissant. On déduplique les SSID
  // pour ne garder que le meilleur point d'accès d'un réseau maillé.
  for (int i = 0; i < result; ++i) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    bool seen = false;
    for (JsonObject item : networks) {
      if (item["ssid"].as<String>() == ssid) {
        seen = true;
        break;
      }
    }
    if (seen) continue;
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = ssid;
    item["rssi"] = WiFi.RSSI(i);
    item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void sendError(int code, const char *message) {
  JsonDocument doc;
  doc["error"] = message;
  String payload;
  serializeJson(doc, payload);
  server.send(code, "application/json", payload);
}

bool validHexColor(const String &value) {
  if (value.length() != 7 || value[0] != '#') return false;
  for (size_t index = 1; index < 7; ++index) {
    char character = value[index];
    if (!isxdigit(static_cast<unsigned char>(character))) return false;
  }
  return true;
}

void handleSave() {
  if (!requireAuth()) return;
  String ssid = server.arg("ssid");
  String name = server.arg("name");
  String password = server.arg("password");
  ssid.trim();
  name.trim();
  int sda = server.arg("sda").toInt();
  int scl = server.arg("scl").toInt();
  int radarARx = server.arg("radar_a_rx").toInt();
  int radarATx = server.arg("radar_a_tx").toInt();
  int radarBRx = server.arg("radar_b_rx").toInt();
  int radarBTx = server.arg("radar_b_tx").toInt();
  int pogHomePort = server.arg("poghome_port").toInt();
  int presenceBrightness = server.arg("presence_light_brightness").toInt();
  int presenceHold = server.arg("presence_light_hold").toInt();
  String presenceColor = server.arg("presence_light_color");
  if (!ssid.length() || ssid.length() > 32) {
    sendError(400, "Le nom du réseau est invalide.");
    return;
  }
  if (password.length() && password.length() < 8) {
    sendError(400, "Le mot de passe doit contenir au moins 8 caractères.");
    return;
  }
  bool statusLightInstalled = server.hasArg("status_light_installed");
  int pins[] = {sda, scl, radarARx, radarATx, radarBRx, radarBTx};
  bool invalidPins = false;
  for (size_t i = 0; i < 6; ++i) {
    invalidPins |= pins[i] < 0 || pins[i] > 48;
    invalidPins |= statusLightInstalled &&
                   pins[i] == POGSENSOR_STATUS_LED_PIN;
    for (size_t j = i + 1; j < 6; ++j) invalidPins |= pins[i] == pins[j];
  }
  if (name.length() > 48 || invalidPins || pogHomePort < 1 ||
      pogHomePort > 65535 || presenceBrightness < 0 ||
      presenceBrightness > 100 || presenceHold < 0 || presenceHold > 300 ||
      !validHexColor(presenceColor)) {
    sendError(400, "Les réglages avancés sont invalides.");
    return;
  }
  g_config.wifiSsid = ssid;
  if (password.length()) g_config.wifiPassword = password;
  g_config.name = name.length() ? name : "Capteur POG";
  g_config.pogHomeHost = server.arg("poghome");
  g_config.pogHomeHost.trim();
  g_config.pogHomePort = pogHomePort;
  g_config.sdaPin = sda;
  g_config.sclPin = scl;
  g_config.radarARxPin = radarARx;
  g_config.radarATxPin = radarATx;
  g_config.radarBRxPin = radarBRx;
  g_config.radarBTxPin = radarBTx;
  g_config.statusLightInstalled = statusLightInstalled;
  g_config.presenceLightAuto = server.hasArg("presence_light_auto");
  g_config.presenceLightBrightness = presenceBrightness;
  g_config.presenceLightColor =
      static_cast<uint32_t>(strtoul(presenceColor.c_str() + 1, nullptr, 16));
  g_config.presenceLightHoldSeconds = presenceHold;
  g_config.samplePeriodSeconds =
      constrain(server.arg("period").toInt(), 5, 3600);
  g_config.temperatureOffset =
      constrain(server.arg("offset").toFloat(), -10.0f, 10.0f);
  if (!configSave()) {
    sendError(500, "La configuration n’a pas pu être enregistrée.");
    return;
  }
  server.send(200, "application/json", "{\"status\":\"saved\"}");
  delay(500);
  ESP.restart();
}

void handleUpdateStatus() {
  if (!requireAuth()) return;
  JsonDocument doc;
  otaUpdateFillJson(doc.to<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", payload);
}

void handleUpdateCheck() {
  if (!requireAuth()) return;
  if (WiFi.status() != WL_CONNECTED) {
    sendError(503, "Le Wi-Fi est indisponible.");
    return;
  }
  otaUpdateRequestCheck();
  server.send(202, "application/json", "{\"ok\":true}");
}

void handleUpdateInstall() {
  if (!requireAuth()) return;
  if (!otaUpdateRequestInstall()) {
    sendError(409, "Aucune mise à jour n’est prête.");
    return;
  }
  server.send(202, "application/json", "{\"ok\":true}");
}

void handleOtaDone() {
  if (!otaUploadAuthorized) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  otaUploadAuthorized = false;
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(200, "application/json",
              ok ? "{\"ok\":true}"
                 : "{\"ok\":false,\"error\":\"La mise à jour a échoué.\"}");
  if (ok) {
    delay(800);
    ESP.restart();
  }
}

void handleOtaUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaUploadAuthorized = requestAuthorized();
    if (!otaUploadAuthorized) return;
    Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
  } else if (!otaUploadAuthorized) {
    return;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void handleReboot() {
  if (!requireAuth()) return;
  server.send(200, "application/json", "{\"ok\":true}");
  delay(300);
  ESP.restart();
}

void registerCaptiveRoutes() {
  // Sondes captives des principaux OS. Une réponse 200 avec la page force
  // l'ouverture de la mini-fenêtre d'onboarding.
  server.on("/hotspot-detect.html", HTTP_ANY, sendSetupPage);  // Apple
  server.on("/library/test/success.html", HTTP_ANY, sendSetupPage);
  server.on("/generate_204", HTTP_ANY, redirectToPortal);      // Android
  server.on("/gen_204", HTTP_ANY, redirectToPortal);
  server.on("/connecttest.txt", HTTP_ANY, redirectToPortal);   // Windows
  server.on("/ncsi.txt", HTTP_ANY, redirectToPortal);
  server.on("/fwlink", HTTP_ANY, redirectToPortal);
  server.on("/canonical.html", HTTP_ANY, redirectToPortal);    // Firefox
}
}  // namespace

void webPortalBegin(bool setupMode) {
  captive = setupMode;
  webAuthBegin();
  const char *headers[] = {"X-Auth-Token"};
  server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, sendSetupPage);
  server.on("/api/auth/status", HTTP_GET, handleAuthStatus);
  server.on("/api/auth/setup", HTTP_POST, handleAuthSetup);
  server.on("/api/auth/login", HTTP_POST, handleAuthLogin);
  server.on("/api/auth/password", HTTP_POST, handleAuthPassword);
  server.on("/api/auth/logout", HTTP_POST, handleAuthLogout);
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/networks", HTTP_GET, sendNetworks);
  server.on("/api/update", HTTP_GET, handleUpdateStatus);
  server.on("/api/update/check", HTTP_POST, handleUpdateCheck);
  server.on("/api/update/install", HTTP_POST, handleUpdateInstall);
  server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/save", HTTP_POST, handleSave);
  registerCaptiveRoutes();
  server.onNotFound(redirectToPortal);
  if (captive) dns.start(53, "*", IPAddress(192, 168, 4, 1));
  server.begin();
  if (!captive) otaUpdateBegin();
  Serial.printf("[Web] %s\n", captive ? "portail captif actif sur 192.168.4.1" :
                                       WiFi.localIP().toString().c_str());
}

void webPortalLoop() {
  if (captive) dns.processNextRequest();
  server.handleClient();
}
