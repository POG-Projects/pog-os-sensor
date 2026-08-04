#include "status_led.h"

#include <Adafruit_NeoPixel.h>

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
