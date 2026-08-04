#pragma once

#include <Arduino.h>

enum class StatusLedCode : uint8_t {
  Boot,
  Setup,
  WifiConnecting,
  PogHomeWaiting,
  Ready,
  UserLight,
  OtaChecking,
  OtaAvailable,
  OtaDownloading,
  OtaVerifying,
  NoSensor,
  EnvironmentError,
  RadarError,
  PogHomeError,
  OtaError,
};

void statusLedBegin(bool enabled);
void statusLedSet(StatusLedCode code);
void statusLedLoop(uint32_t now);
void statusLedSetUserLight(uint32_t color, uint8_t brightnessPercent);
StatusLedCode statusLedCode();
bool statusLedEnabled();
const char *statusLedCodeName(StatusLedCode code);
