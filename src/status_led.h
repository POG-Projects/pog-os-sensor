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
bool statusLedSetUserEffect(const char *name, uint8_t speed,
                            uint8_t brightnessPercent, float primaryHue,
                            float primarySaturation, float secondaryHue,
                            float secondarySaturation, uint32_t now);
void statusLedClearUserEffect();
bool statusLedEffectSyncJoin(const char *groupId, const char *leaderEntityId,
                             uint16_t presentationDelayMs,
                             int16_t calibrationOffsetMs,
                             const char *visualizer);
bool statusLedEffectSyncLeave(const char *groupId);
void statusLedEffectSyncCancel();
bool statusLedEffectSyncFrame(uint32_t seq, uint64_t monoMs,
                              uint64_t presentAtMs, uint16_t leadMs,
                              float level, float bass, float treble,
                              uint32_t receivedMs);
StatusLedCode statusLedCode();
bool statusLedEnabled();
const char *statusLedCodeName(StatusLedCode code);
