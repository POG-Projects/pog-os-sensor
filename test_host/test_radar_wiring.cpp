#include <cassert>

#include "radar_wiring.h"

using pogsensor::radar::WiringStatus;
using pogsensor::radar::classifyWiring;

int main() {
  assert(classifyWiring(true, false) == WiringStatus::Correct);
  assert(classifyWiring(true, true) == WiringStatus::Correct);
  assert(classifyWiring(false, true) == WiringStatus::Reversed);
  assert(classifyWiring(false, false) == WiringStatus::NoSignal);
  return 0;
}
