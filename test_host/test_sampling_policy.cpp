#include <cassert>

#include "sampling_policy.h"

using pogsensor::Reading;
using pogsensor::finiteReading;
using pogsensor::materiallyChanged;

int main() {
  Reading base{21.0f, 45.0f, 1013.0f, -55, true, true};
  assert(finiteReading(base));
  assert(!materiallyChanged(base, Reading{21.1f, 45.5f, 1013.5f, -53, true, true}));
  assert(materiallyChanged(base, Reading{21.2f, 45.0f, 1013.0f, -55, true, true}));
  assert(materiallyChanged(base, Reading{21.0f, 46.0f, 1013.0f, -55, true, true}));
  assert(materiallyChanged(base, Reading{21.0f, 45.0f, 1014.0f, -55, true, true}));
  assert(materiallyChanged(base, Reading{21.0f, 45.0f, 1013.0f, -60, true, true}));
  Reading offline = base;
  offline.sensorOnline = false;
  assert(materiallyChanged(base, offline));
  return 0;
}
