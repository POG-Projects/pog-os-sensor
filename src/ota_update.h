#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Vérifie automatiquement la dernière release GitHub une fois le Wi-Fi prêt,
// puis toutes les six heures. L'installation reste confirmée depuis le portail.
void otaUpdateBegin();
void otaUpdateRequestCheck();
bool otaUpdateRequestInstall();
void otaUpdateFillJson(JsonObject out);
