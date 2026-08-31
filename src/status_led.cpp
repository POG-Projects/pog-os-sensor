#include "status_led.h"

#include <Adafruit_NeoPixel.h>
#include <sys/time.h>

#include "effect_sync.h"

#ifndef POGSENSOR_STATUS_LED_PIN
#define POGSENSOR_STATUS_LED_PIN 7
#endif
#ifndef POGSENSOR_STATUS_LED_COUNT
#define POGSENSOR_STATUS_LED_COUNT 4
#endif

namespace {
constexpr uint16_t kPixelCount = POGSENSOR_STATUS_LED_COUNT;
constexpr uint8_t kMaximumLevel = 96;  // 38 % : environ 23 mA au blanc.
Adafruit_NeoPixel pixel(kPixelCount, POGSENSOR_STATUS_LED_PIN,
                        NEO_GRB + NEO_KHZ800);
StatusLedCode currentCode = StatusLedCode::Boot;
uint32_t stateStarted = 0;
uint32_t previousColor = UINT32_MAX;
bool started = false;
uint32_t userColor = 0xFFD28A;
uint8_t userBrightness = 55;

uint64_t unixClockMs() {
  timeval now = {};
  gettimeofday(&now, nullptr);
  if (now.tv_sec <= 1700000000) return 0;
  return static_cast<uint64_t>(now.tv_sec) * 1000ULL +
         static_cast<uint64_t>(now.tv_usec) / 1000ULL;
}
enum class UserEffect : uint8_t {
  None,
  Solid,
  Rainbow,
  Breathe,
  Comet,
  Twinkle,
};
UserEffect userEffect = UserEffect::None;
uint32_t userSecondaryColor = 0x6C3BFF;
uint8_t userEffectSpeed = 50;
uint32_t userEffectStarted = 0;
pogdev::EffectSync effectSync;

uint8_t triangle(uint32_t elapsed, uint32_t period, uint8_t minimum,
                 uint8_t maximum) {
  uint32_t phase = elapsed % period;
  uint32_t half = period / 2;
  uint32_t ramp = phase < half ? phase : period - phase;
  return minimum + static_cast<uint32_t>(maximum - minimum) * ramp / half;
}

uint32_t rgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
  level = min(level, kMaximumLevel);
  return pixel.gamma32(pixel.Color(
      static_cast<uint16_t>(red) * level / 255,
      static_cast<uint16_t>(green) * level / 255,
      static_cast<uint16_t>(blue) * level / 255));
}

uint32_t hsColor(float hue, float saturation) {
  while (hue < 0) hue += 360.0f;
  while (hue >= 360.0f) hue -= 360.0f;
  saturation = constrain(saturation, 0.0f, 100.0f);
  return pixel.ColorHSV(static_cast<uint16_t>(hue / 360.0f * 65535.0f),
                        static_cast<uint8_t>(saturation * 2.55f), 255);
}

uint32_t hash32(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  return value ^ (value >> 16);
}

uint32_t scaleColor(uint32_t color, uint8_t level) {
  return rgb((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, level);
}

uint32_t renderUserEffect(uint16_t index, uint32_t now) {
  const uint32_t elapsed = now - userEffectStarted;
  const uint8_t maximum = static_cast<uint16_t>(userBrightness) *
                          kMaximumLevel / 100;
  switch (userEffect) {
    case UserEffect::Rainbow: {
      uint16_t hue = static_cast<uint16_t>(
          (static_cast<uint32_t>(index) * 65535U / kPixelCount +
           elapsed * (3U + userEffectSpeed) / 6U) & 0xffffU);
      return pixel.gamma32(pixel.ColorHSV(hue, 255, maximum));
    }
    case UserEffect::Breathe: {
      uint32_t period = 9000U - static_cast<uint32_t>(userEffectSpeed) * 70U;
      if (period < 1200U) period = 1200U;
      uint8_t level = triangle(elapsed, period,
                               maximum / 8, maximum);
      return scaleColor(userColor, level);
    }
    case UserEffect::Comet: {
      uint32_t step = 180U - static_cast<uint32_t>(userEffectSpeed) * 13U / 10U;
      if (step < 25U) step = 25U;
      uint16_t head = (elapsed / step) % kPixelCount;
      uint16_t distance = index > head ? index - head : head - index;
      if (distance > kPixelCount / 2) distance = kPixelCount - distance;
      if (distance == 0) return scaleColor(userColor, maximum);
      if (distance == 1) return scaleColor(userColor, maximum * 3 / 5);
      return scaleColor(userSecondaryColor, maximum / 8);
    }
    case UserEffect::Twinkle: {
      uint32_t period = 700U - static_cast<uint32_t>(userEffectSpeed) * 6U;
      if (period < 80U) period = 80U;
      uint8_t sparkle = hash32(0x504f4700U ^ index ^ (elapsed / period)) & 0xff;
      return scaleColor(sparkle > 205 ? userColor : userSecondaryColor,
                        sparkle > 205 ? maximum : maximum / 7);
    }
    case UserEffect::Solid:
    case UserEffect::None:
    default:
      return scaleColor(userColor, maximum);
  }
}

uint32_t renderSharedAudio(uint16_t index, const pogdev::EffectFrame &frame,
                           pogdev::EffectVisualizer visualizer) {
  const float position = pogdev::effectPixelPosition(index, kPixelCount);
  const pogdev::EffectPixel shared =
      pogdev::effectVisualizerPixel(visualizer, position, frame);
  const uint8_t maximum = static_cast<uint16_t>(userBrightness) *
                          kMaximumLevel / 100;
  return pixel.gamma32(pixel.ColorHSV(
      static_cast<uint16_t>(shared.hue * 65535.0f),
      static_cast<uint8_t>(shared.saturation * 255.0f),
      static_cast<uint8_t>(shared.value * maximum)));
}

bool codedBlink(uint32_t elapsed, uint8_t count, uint32_t cycle = 2600) {
  uint32_t phase = elapsed % cycle;
  constexpr uint32_t kSlot = 320;
  constexpr uint32_t kOn = 140;
  return phase < static_cast<uint32_t>(count) * kSlot &&
         phase % kSlot < kOn;
}

bool doubleBlink(uint32_t elapsed, uint32_t cycle = 1600) {
  uint32_t phase = elapsed % cycle;
  return phase < 160 || (phase >= 340 && phase < 500);
}

uint32_t render(StatusLedCode code, uint32_t elapsed) {
  switch (code) {
    case StatusLedCode::Boot:
      return rgb(255, 255, 255, triangle(elapsed, 1200, 12, 76));
    case StatusLedCode::Setup:
      return rgb(255, 138, 0, doubleBlink(elapsed) ? 88 : 3);
    case StatusLedCode::WifiConnecting:
      return rgb(0, 199, 255, triangle(elapsed, 1100, 3, 88));
    case StatusLedCode::PogHomeWaiting:
      return rgb(139, 92, 246, triangle(elapsed, 1800, 3, 76));
    case StatusLedCode::Ready:
      return rgb(52, 199, 89, triangle(elapsed, 3200, 5, 48));
    case StatusLedCode::UserLight:
      return rgb((userColor >> 16) & 0xff, (userColor >> 8) & 0xff,
                 userColor & 0xff,
                 static_cast<uint16_t>(userBrightness) * kMaximumLevel / 100);
    case StatusLedCode::OtaChecking:
      return rgb(10, 132, 255, (elapsed / 120) % 2 == 0 ? 88 : 2);
    case StatusLedCode::OtaAvailable:
      return rgb(100, 210, 255, doubleBlink(elapsed, 2400) ? 80 : 3);
    case StatusLedCode::OtaDownloading:
      return rgb(0, 122, 255, triangle(elapsed, 650, 4, 96));
    case StatusLedCode::OtaVerifying:
      return rgb(255, 255, 255, (elapsed / 100) % 2 == 0 ? 96 : 2);
    case StatusLedCode::NoSensor:
      return rgb(255, 59, 48, codedBlink(elapsed, 1) ? 96 : 2);
    case StatusLedCode::EnvironmentError:
      return rgb(255, 59, 48, codedBlink(elapsed, 2) ? 96 : 2);
    case StatusLedCode::RadarError:
      return rgb(255, 59, 48, codedBlink(elapsed, 3) ? 96 : 2);
    case StatusLedCode::PogHomeError:
      return rgb(175, 82, 222, codedBlink(elapsed, 3) ? 96 : 2);
    case StatusLedCode::OtaError:
      return rgb(255, 45, 85, codedBlink(elapsed, 4, 3000) ? 96 : 2);
  }
  return 0;
}
}  // namespace

void statusLedBegin(bool enabled) {
  if (!enabled) {
    started = false;
    return;
  }
  pixel.begin();
  pixel.clear();
  pixel.show();
  started = true;
  stateStarted = millis();
  previousColor = UINT32_MAX;
  statusLedLoop(stateStarted);
}

void statusLedSet(StatusLedCode code) {
  if (currentCode == code) return;
  currentCode = code;
  stateStarted = millis();
  previousColor = UINT32_MAX;
  Serial.printf("[LED] %s\n", statusLedCodeName(code));
}

void statusLedLoop(uint32_t now) {
  if (!started) return;
  pogdev::EffectFrame shared;
  if (currentCode == StatusLedCode::UserLight &&
      pogdev::effectSyncSample(effectSync, now, unixClockMs(), shared)) {
    for (uint16_t index = 0; index < kPixelCount; ++index) {
      pixel.setPixelColor(
          index, renderSharedAudio(index, shared, effectSync.visualizer));
    }
    pixel.show();
    return;
  }
  if (currentCode == StatusLedCode::UserLight &&
      userEffect != UserEffect::None) {
    for (uint16_t index = 0; index < kPixelCount; ++index) {
      pixel.setPixelColor(index, renderUserEffect(index, now));
    }
    pixel.show();
    return;
  }
  uint32_t color = render(currentCode, now - stateStarted);
  if (color == previousColor) return;
  previousColor = color;
  for (uint16_t index = 0; index < kPixelCount; ++index) {
    pixel.setPixelColor(index, color);
  }
  pixel.show();
}

void statusLedSetUserLight(uint32_t color, uint8_t brightnessPercent) {
  color &= 0xFFFFFF;
  brightnessPercent = min(brightnessPercent, static_cast<uint8_t>(100));
  if (userColor == color && userBrightness == brightnessPercent) return;
  userColor = color;
  userBrightness = brightnessPercent;
  if (currentCode == StatusLedCode::UserLight) previousColor = UINT32_MAX;
}

bool statusLedSetUserEffect(const char *name, uint8_t speed,
                            uint8_t brightnessPercent, float primaryHue,
                            float primarySaturation, float secondaryHue,
                            float secondarySaturation, uint32_t now) {
  if (!name) return false;
  UserEffect effect = UserEffect::None;
  if (strcmp(name, "Uni") == 0) effect = UserEffect::Solid;
  else if (strcmp(name, "Arc-en-ciel") == 0) effect = UserEffect::Rainbow;
  else if (strcmp(name, "Respiration") == 0) effect = UserEffect::Breathe;
  else if (strcmp(name, "Comète") == 0) effect = UserEffect::Comet;
  else if (strcmp(name, "Scintillement") == 0) effect = UserEffect::Twinkle;
  else return false;
  userEffect = effect;
  userEffectSpeed = min(speed, static_cast<uint8_t>(100));
  userBrightness = min(brightnessPercent, static_cast<uint8_t>(100));
  userColor = hsColor(primaryHue, primarySaturation);
  userSecondaryColor = hsColor(secondaryHue, secondarySaturation);
  userEffectStarted = now;
  previousColor = UINT32_MAX;
  return true;
}

void statusLedClearUserEffect() {
  userEffect = UserEffect::None;
  previousColor = UINT32_MAX;
}

bool statusLedEffectSyncJoin(const char *groupId, const char *leaderEntityId,
                             uint16_t presentationDelayMs,
                             int16_t calibrationOffsetMs,
                             const char *visualizer) {
  return pogdev::effectSyncJoin(effectSync, groupId, "follower",
                                leaderEntityId, presentationDelayMs,
                                calibrationOffsetMs, visualizer);
}

bool statusLedEffectSyncLeave(const char *groupId) {
  return pogdev::effectSyncLeave(effectSync, groupId);
}

void statusLedEffectSyncCancel() { effectSync = pogdev::EffectSync{}; }

bool statusLedEffectSyncFrame(uint32_t seq, uint64_t monoMs,
                              uint64_t presentAtMs, uint16_t leadMs,
                              float level, float bass, float treble,
                              uint32_t receivedMs) {
  return pogdev::effectSyncPush(effectSync, seq, monoMs, presentAtMs, leadMs,
                                level, bass, treble, receivedMs);
}

StatusLedCode statusLedCode() { return currentCode; }
bool statusLedEnabled() { return started; }

const char *statusLedCodeName(StatusLedCode code) {
  switch (code) {
    case StatusLedCode::Boot:
      return "S10 démarrage";
    case StatusLedCode::Setup:
      return "S20 configuration";
    case StatusLedCode::WifiConnecting:
      return "S30 connexion Wi-Fi";
    case StatusLedCode::PogHomeWaiting:
      return "S40 attente POG Home";
    case StatusLedCode::Ready:
      return "S00 opérationnel";
    case StatusLedCode::UserLight:
      return "S60 lumière de présence";
    case StatusLedCode::OtaChecking:
      return "S50 vérification OTA";
    case StatusLedCode::OtaAvailable:
      return "S51 mise à jour disponible";
    case StatusLedCode::OtaDownloading:
      return "S52 téléchargement OTA";
    case StatusLedCode::OtaVerifying:
      return "S53 vérification OTA";
    case StatusLedCode::NoSensor:
      return "E10 aucun capteur";
    case StatusLedCode::EnvironmentError:
      return "E11 lecture environnement";
    case StatusLedCode::RadarError:
      return "E12 liaison radar";
    case StatusLedCode::PogHomeError:
      return "E30 POG Home indisponible";
    case StatusLedCode::OtaError:
      return "E40 échec OTA";
  }
  return "inconnu";
}
