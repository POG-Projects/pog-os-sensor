#pragma once

#include <math.h>

struct PresenceLightRequest {
  bool hasBrightness = false;
  float brightness = 0;
  bool hasHue = false;
  float hue = 0;
  bool hasSaturation = false;
  float saturation = 0;
  bool hasKelvin = false;
  float kelvin = 0;
  float transitionSeconds = 0;
};

inline bool validPresenceLightRequest(const PresenceLightRequest &request) {
  return isfinite(request.transitionSeconds) &&
         request.transitionSeconds >= 0 && request.transitionSeconds <= 300 &&
         (!request.hasBrightness ||
          (isfinite(request.brightness) && request.brightness >= 1 &&
           request.brightness <= 100)) &&
         request.hasHue == request.hasSaturation &&
         (!request.hasHue ||
          (isfinite(request.hue) && request.hue >= 0 && request.hue <= 360 &&
           isfinite(request.saturation) && request.saturation >= 0 &&
           request.saturation <= 100)) &&
         (!request.hasKelvin ||
          (isfinite(request.kelvin) && request.kelvin >= 1700 &&
           request.kelvin <= 6535)) &&
         !(request.hasHue && request.hasKelvin);
}
