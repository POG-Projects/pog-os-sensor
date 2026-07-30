#include "web_portal.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "environment_sensor.h"
#include "ota_update.h"
#include "pogdev.h"
#include "web_ui.h"

namespace {
WebServer server(80);
DNSServer dns;
bool captive = false;

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
  JsonDocument doc;
  char address[8] = "";
  if (environmentSensorAddress()) {
    snprintf(address, sizeof(address), "0x%02X", environmentSensorAddress());
  }
  doc["sensor"] = environmentSensorModel();
  doc["sensor_online"] = environmentSensorKind() != SensorKind::None;
  doc["address"] = address;
  doc["hw_id"] = pogdevHardwareId();
  doc["poghome_status"] = pogdevIsConnected() ? "connecté à POG Home" :
                              pogdevIsAdopted() ? "adopté" :
                                                "en attente d’adoption";
  doc["ssid"] = g_config.wifiSsid;
  doc["name"] = g_config.name;
  doc["poghome"] = g_config.pogHomeHost;
  doc["sda"] = g_config.sdaPin;
  doc["scl"] = g_config.sclPin;
  doc["period"] = g_config.samplePeriodSeconds;
  doc["offset"] = g_config.temperatureOffset;
  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void sendNetworks() {
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

void handleSave() {
  String ssid = server.arg("ssid");
  String name = server.arg("name");
  String password = server.arg("password");
  ssid.trim();
  name.trim();
  int sda = server.arg("sda").toInt();
  int scl = server.arg("scl").toInt();
  if (!ssid.length() || ssid.length() > 32) {
    sendError(400, "Le nom du réseau est invalide.");
    return;
  }
  if (password.length() && password.length() < 8) {
    sendError(400, "Le mot de passe doit contenir au moins 8 caractères.");
    return;
  }
  if (name.length() > 48 || sda == scl || sda < 0 || sda > 48 ||
      scl < 0 || scl > 48) {
    sendError(400, "Les réglages avancés sont invalides.");
    return;
  }
  g_config.wifiSsid = ssid;
  if (password.length()) g_config.wifiPassword = password;
  g_config.name = name.length() ? name : "Capteur POG";
  g_config.pogHomeHost = server.arg("poghome");
  g_config.pogHomeHost.trim();
  g_config.sdaPin = sda;
  g_config.sclPin = scl;
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
  JsonDocument doc;
  otaUpdateFillJson(doc.to<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", payload);
}

void handleUpdateCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    sendError(503, "Le Wi-Fi est indisponible.");
    return;
  }
  otaUpdateRequestCheck();
  server.send(202, "application/json", "{\"ok\":true}");
}

void handleUpdateInstall() {
  if (!otaUpdateRequestInstall()) {
    sendError(409, "Aucune mise à jour n’est prête.");
    return;
  }
  server.send(202, "application/json", "{\"ok\":true}");
}

void handleOtaDone() {
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
    Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void handleReboot() {
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
  server.on("/", HTTP_GET, sendSetupPage);
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
