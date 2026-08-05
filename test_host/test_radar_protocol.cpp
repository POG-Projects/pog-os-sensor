#include <cassert>
#include <cstdint>

#include "radar_protocol.h"

int main() {
  uint8_t ld2450[30] = {
      0xAA, 0xFF, 0x03, 0x00,
      0x0E, 0x03, 0xB1, 0x86, 0x10, 0x00, 0x40, 0x01,
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0,
      0x55, 0xCC};
  pogsensor::radar::Target targets[3];
  uint8_t count = 0;
  assert(pogsensor::radar::parseLd2450(ld2450, sizeof(ld2450), targets,
                                       count));
  assert(count == 1);
  assert(targets[0].xMm == -782);
  assert(targets[0].yMm == 1713);
  assert(targets[0].speedCmS == -16);
  assert(targets[0].resolutionMm == 320);

  uint8_t ld2410[23] = {
      0xF4, 0xF3, 0xF2, 0xF1, 0x0D, 0x00,
      0x02, 0xAA, 0x03, 0x7B, 0x00, 70, 0xC8, 0x01, 55,
      0xF4, 0x01, 0x55, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};
  pogsensor::radar::Presence presence;
  assert(pogsensor::radar::parseLd2410(ld2410, sizeof(ld2410), presence));
  assert(presence.occupied && presence.motion);
  assert(presence.movingDistanceCm == 123);
  assert(presence.stationaryDistanceCm == 456);
  assert(presence.detectionDistanceCm == 500);
  return 0;
}
