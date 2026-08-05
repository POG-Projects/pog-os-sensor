#pragma once

#include <cstddef>
#include <cstdint>

namespace pogsensor::radar {

struct Target {
  bool active = false;
  int16_t xMm = 0;
  int16_t yMm = 0;
  int16_t speedCmS = 0;
  uint16_t resolutionMm = 0;
};

struct Presence {
  bool occupied = false;
  bool motion = false;
  uint16_t movingDistanceCm = 0;
  uint16_t stationaryDistanceCm = 0;
  uint16_t detectionDistanceCm = 0;
};

inline uint16_t littleU16(const uint8_t *data) {
  return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
}

// Le LD2450 n'utilise pas le complément à deux : le bit haut vaut 1 pour une
// grandeur positive et 0 pour une grandeur négative.
inline int16_t ld2450Signed(const uint8_t *data) {
  uint16_t raw = littleU16(data);
  int16_t magnitude = raw & 0x7fff;
  return raw & 0x8000 ? magnitude : -magnitude;
}

inline bool parseLd2450(const uint8_t *frame, size_t size, Target targets[3],
                        uint8_t &count) {
  if (size != 30 || frame[0] != 0xAA || frame[1] != 0xFF ||
      frame[2] != 0x03 || frame[3] != 0x00 || frame[28] != 0x55 ||
      frame[29] != 0xCC) return false;
  count = 0;
  for (size_t i = 0; i < 3; ++i) {
    const uint8_t *raw = frame + 4 + i * 8;
    bool active = false;
    for (size_t byte = 0; byte < 8; ++byte) active |= raw[byte] != 0;
    targets[i] = Target{};
    if (!active) continue;
    targets[i].active = true;
    targets[i].xMm = ld2450Signed(raw);
    targets[i].yMm = ld2450Signed(raw + 2);
    targets[i].speedCmS = ld2450Signed(raw + 4);
    targets[i].resolutionMm = littleU16(raw + 6);
    ++count;
  }
  return true;
}

inline bool parseLd2410(const uint8_t *frame, size_t size, Presence &presence) {
  if (size < 23 || frame[0] != 0xF4 || frame[1] != 0xF3 ||
      frame[2] != 0xF2 || frame[3] != 0xF1) return false;
  uint16_t payloadSize = littleU16(frame + 4);
  if (payloadSize < 13 || size != size_t{4 + 2 + 4} + payloadSize) return false;
  const uint8_t *footer = frame + size - 4;
  if (footer[0] != 0xF8 || footer[1] != 0xF7 || footer[2] != 0xF6 ||
      footer[3] != 0xF5) return false;
  const uint8_t *payload = frame + 6;
  if ((payload[0] != 0x01 && payload[0] != 0x02) || payload[1] != 0xAA) {
    return false;
  }
  uint8_t state = payload[2];
  presence.occupied = state != 0;
  presence.motion = state == 1 || state == 3;
  presence.movingDistanceCm = littleU16(payload + 3);
  presence.stationaryDistanceCm = littleU16(payload + 6);
  presence.detectionDistanceCm = littleU16(payload + 9);
  return true;
}

}  // namespace pogsensor::radar
