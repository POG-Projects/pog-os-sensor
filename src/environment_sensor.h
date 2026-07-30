#pragma once

#include <Arduino.h>

#include "sampling_policy.h"

enum class SensorKind { None, Bme280, Bmp280 };

bool environmentSensorBegin();
bool environmentSensorRead(pogsensor::Reading &reading);
SensorKind environmentSensorKind();
const char *environmentSensorModel();
uint8_t environmentSensorAddress();
