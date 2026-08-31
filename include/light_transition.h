#pragma once

#include <stdint.h>

namespace pogsensor {

struct LightTransition {
  bool active = false;
  uint32_t startedAt = 0;
  uint32_t durationMs = 0;
  uint8_t fromBrightness = 0;
  uint8_t toBrightness = 0;
  uint32_t fromColor = 0;
  uint32_t toColor = 0;
};

inline uint8_t interpolateChannel(uint8_t from, uint8_t to,
                                  uint32_t elapsed, uint32_t duration) {
  if (!duration || elapsed >= duration) return to;
  int32_t delta = static_cast<int32_t>(to) - static_cast<int32_t>(from);
  int32_t value = static_cast<int32_t>(from) +
                  static_cast<int32_t>(static_cast<int64_t>(delta) * elapsed /
                                       duration);
  return static_cast<uint8_t>(value);
}

inline uint32_t interpolateColor(uint32_t from, uint32_t to,
                                 uint32_t elapsed, uint32_t duration) {
  uint8_t red = interpolateChannel((from >> 16) & 0xff, (to >> 16) & 0xff,
                                   elapsed, duration);
  uint8_t green = interpolateChannel((from >> 8) & 0xff, (to >> 8) & 0xff,
                                     elapsed, duration);
  uint8_t blue = interpolateChannel(from & 0xff, to & 0xff, elapsed, duration);
  return (static_cast<uint32_t>(red) << 16) |
         (static_cast<uint32_t>(green) << 8) | blue;
}

inline void startLightTransition(LightTransition &transition, uint32_t now,
                                 uint32_t durationMs, uint8_t fromBrightness,
                                 uint8_t toBrightness, uint32_t fromColor,
                                 uint32_t toColor, uint8_t &brightness,
                                 uint32_t &color) {
  transition.startedAt = now;
  transition.durationMs = durationMs;
  transition.fromBrightness = fromBrightness;
  transition.toBrightness = toBrightness;
  transition.fromColor = fromColor & 0xFFFFFF;
  transition.toColor = toColor & 0xFFFFFF;
  transition.active = durationMs != 0 &&
                      (fromBrightness != toBrightness ||
                       transition.fromColor != transition.toColor);
  brightness = transition.active ? fromBrightness : toBrightness;
  color = transition.active ? transition.fromColor : transition.toColor;
}

inline bool advanceLightTransition(LightTransition &transition, uint32_t now,
                                   uint8_t &brightness, uint32_t &color) {
  if (!transition.active) return false;
  // Unsigned subtraction deliberately keeps transitions correct across the
  // millis() wrap-around.
  uint32_t elapsed = now - transition.startedAt;
  brightness = interpolateChannel(transition.fromBrightness,
                                  transition.toBrightness, elapsed,
                                  transition.durationMs);
  color = interpolateColor(transition.fromColor, transition.toColor, elapsed,
                           transition.durationMs);
  if (elapsed >= transition.durationMs) transition.active = false;
  return transition.active;
}

}  // namespace pogsensor
