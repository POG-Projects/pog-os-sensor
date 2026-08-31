#include <cassert>
#include <limits>

#include "presence_light_request.h"

int main() {
  PresenceLightRequest request;
  assert(validPresenceLightRequest(request));  // Empty means turn on.

  request.hasBrightness = true;
  request.brightness = 1;
  assert(validPresenceLightRequest(request));
  request.brightness = 0;
  assert(!validPresenceLightRequest(request));
  request.brightness = 101;
  assert(!validPresenceLightRequest(request));

  request = PresenceLightRequest{};
  request.hasHue = true;
  request.hue = 120;
  assert(!validPresenceLightRequest(request));
  request.hasSaturation = true;
  request.saturation = 80;
  assert(validPresenceLightRequest(request));
  request.hasKelvin = true;
  request.kelvin = 2700;
  assert(!validPresenceLightRequest(request));

  request = PresenceLightRequest{};
  request.hasKelvin = true;
  request.kelvin = 1700;
  request.transitionSeconds = 300;
  assert(validPresenceLightRequest(request));
  request.kelvin = 6536;
  assert(!validPresenceLightRequest(request));
  request.kelvin = 2700;
  request.transitionSeconds = std::numeric_limits<float>::quiet_NaN();
  assert(!validPresenceLightRequest(request));

  return 0;
}
