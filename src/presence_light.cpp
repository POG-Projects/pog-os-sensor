#include "presence_light.h"

#include <math.h>

#include "config.h"

namespace {
bool manualOn = false;
bool outputOn = false;
bool occupiedNow = false;
bool suppressUntilClear = false;
uint32_t lastPresenceAt = 0;

uint8_t channel(float value) {
  return static_cast<uint8_t>(roundf(constrain(value, 0.0f, 255.0f)));
}

uint32_t packRgb(uint8_t red, uint8_t green, uint8_t blue) {
  return (static_cast<uint32_t>(red) << 16) |
         (static_cast<uint32_t>(green) << 8) | blue;
}

void rgbToHs(uint32_t color, float &hue, float &saturation) {
  float red = ((color >> 16) & 0xff) / 255.0f;
  float green = ((color >> 8) & 0xff) / 255.0f;
  float blue = (color & 0xff) / 255.0f;
  float maximum = max(red, max(green, blue));
  float minimum = min(red, min(green, blue));
  float delta = maximum - minimum;
  saturation = maximum == 0 ? 0 : delta / maximum * 100.0f;
  if (delta == 0) {
    hue = 0;
  } else if (maximum == red) {
    hue = 60.0f * fmodf((green - blue) / delta, 6.0f);
  } else if (maximum == green) {
    hue = 60.0f * ((blue - red) / delta + 2.0f);
  } else {
    hue = 60.0f * ((red - green) / delta + 4.0f);
  }
  if (hue < 0) hue += 360.0f;
}

uint32_t hsToRgb(float hue, float saturation) {
  hue = fmodf(max(0.0f, hue), 360.0f);
  saturation = constrain(saturation, 0.0f, 100.0f) / 100.0f;
  float chroma = saturation;
  float x = chroma * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
  float red = 0, green = 0, blue = 0;
  if (hue < 60) red = chroma, green = x;
  else if (hue < 120) red = x, green = chroma;
  else if (hue < 180) green = chroma, blue = x;
  else if (hue < 240) green = x, blue = chroma;
  else if (hue < 300) red = x, blue = chroma;
  else red = chroma, blue = x;
  float match = 1.0f - chroma;
  return packRgb(channel((red + match) * 255.0f),
                 channel((green + match) * 255.0f),
                 channel((blue + match) * 255.0f));
}
}  // namespace

void presenceLightBegin() {
  manualOn = false;
  outputOn = false;
  occupiedNow = false;
  suppressUntilClear = false;
  lastPresenceAt = 0;
}

bool presenceLightUpdate(bool radarOnline, bool occupied, uint32_t now) {
  bool previous = outputOn;
  if (!g_config.statusLightInstalled) {
    occupiedNow = false;
    outputOn = false;
    return outputOn != previous;
  }
  occupiedNow = radarOnline && occupied;
  if (occupiedNow) {
    lastPresenceAt = now;
  } else if (suppressUntilClear) {
    suppressUntilClear = false;
    lastPresenceAt = 0;
  }

  uint32_t holdMs = static_cast<uint32_t>(g_config.presenceLightHoldSeconds) *
                    1000UL;
  bool held = lastPresenceAt && now - lastPresenceAt <= holdMs;
  bool automaticOn = g_config.presenceLightAuto && !suppressUntilClear &&
                     (occupiedNow || held);
  outputOn = manualOn || automaticOn;
  return outputOn != previous;
}

bool presenceLightIsOn() { return outputOn; }
bool presenceLightAutomatic() { return g_config.presenceLightAuto; }
uint8_t presenceLightBrightness() {
  return g_config.presenceLightBrightness;
}
uint32_t presenceLightColor() { return g_config.presenceLightColor; }
uint16_t presenceLightHoldSeconds() {
  return g_config.presenceLightHoldSeconds;
}

void presenceLightTurnOn() {
  manualOn = true;
  suppressUntilClear = false;
  outputOn = true;
}

void presenceLightTurnOff() {
  manualOn = false;
  suppressUntilClear = occupiedNow;
  lastPresenceAt = 0;
  outputOn = false;
}

void presenceLightToggle() {
  if (outputOn) presenceLightTurnOff();
  else presenceLightTurnOn();
}

void presenceLightSetAutomatic(bool enabled) {
  g_config.presenceLightAuto = enabled;
  if (!enabled) {
    suppressUntilClear = false;
    lastPresenceAt = 0;
    outputOn = manualOn;
  } else {
    outputOn = manualOn || (!suppressUntilClear && occupiedNow);
  }
}

void presenceLightSetBrightness(float percent) {
  g_config.presenceLightBrightness = static_cast<uint8_t>(
      roundf(constrain(percent, 0.0f, 100.0f)));
}

void presenceLightSetHs(float hue, float saturation) {
  g_config.presenceLightColor = hsToRgb(hue, saturation);
}

void presenceLightSetColorTemperature(float kelvin) {
  float temperature = constrain(kelvin, 1000.0f, 10000.0f) / 100.0f;
  float red = temperature <= 66.0f
                  ? 255.0f
                  : 329.698727446f * powf(temperature - 60.0f, -0.1332047592f);
  float green = temperature <= 66.0f
                    ? 99.4708025861f * logf(temperature) - 161.1195681661f
                    : 288.1221695283f * powf(temperature - 60.0f,
                                             -0.0755148492f);
  float blue = temperature >= 66.0f
                   ? 255.0f
                   : temperature <= 19.0f
                         ? 0.0f
                         : 138.5177312231f * logf(temperature - 10.0f) -
                               305.0447927307f;
  g_config.presenceLightColor =
      packRgb(channel(red), channel(green), channel(blue));
}

void presenceLightSetHoldSeconds(float seconds) {
  g_config.presenceLightHoldSeconds = static_cast<uint16_t>(
      roundf(constrain(seconds, 0.0f, 300.0f)));
}

void presenceLightFillState(JsonObject light, JsonObject automatic,
                            JsonObject hold) {
  float hue, saturation;
  rgbToHs(g_config.presenceLightColor, hue, saturation);
  light["on"] = outputOn;
  light["brightness"] = g_config.presenceLightBrightness;
  light["mode"] = "hs";
  light["hue"] = roundf(hue * 10.0f) / 10.0f;
  light["saturation"] = roundf(saturation * 10.0f) / 10.0f;
  automatic["on"] = g_config.presenceLightAuto;
  hold["value"] = g_config.presenceLightHoldSeconds;
}
