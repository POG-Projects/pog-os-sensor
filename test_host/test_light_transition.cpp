#include <cassert>
#include <cstdint>

#include "light_transition.h"

using pogsensor::LightTransition;
using pogsensor::advanceLightTransition;
using pogsensor::startLightTransition;

int main() {
  LightTransition transition;
  uint8_t brightness = 0;
  uint32_t color = 0;

  startLightTransition(transition, 1000, 2000, 0, 80, 0x000000,
                       0xC86432, brightness, color);
  assert(transition.active);
  assert(brightness == 0);
  assert(color == 0x000000);

  assert(advanceLightTransition(transition, 2000, brightness, color));
  assert(brightness == 40);
  assert(color == 0x643219);

  assert(!advanceLightTransition(transition, 3000, brightness, color));
  assert(brightness == 80);
  assert(color == 0xC86432);

  // A zero-duration gesture is applied immediately and never schedules work.
  startLightTransition(transition, 4000, 0, 20, 75, 0x112233, 0xAABBCC,
                       brightness, color);
  assert(!transition.active);
  assert(brightness == 75);
  assert(color == 0xAABBCC);

  // millis() wrapping must not interrupt a long-running fade.
  startLightTransition(transition, UINT32_MAX - 499, 1000, 10, 90, 0x000000,
                       0x646464, brightness, color);
  assert(advanceLightTransition(transition, 0, brightness, color));
  assert(brightness == 50);
  assert(color == 0x323232);
  assert(!advanceLightTransition(transition, 500, brightness, color));
  assert(brightness == 90);
  assert(color == 0x646464);

  return 0;
}
