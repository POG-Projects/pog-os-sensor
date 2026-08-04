#pragma once

#include <Arduino.h>

#include "sampling_policy.h"

bool environmentSensorBegin();
void environmentSensorLoop();
bool environmentSensorRead(pogsensor::Reading &reading);
const char *environmentSensorModel();
const char *environmentSensorAddresses();
bool environmentSensorPresent();
bool environmentHasTemperature();
bool environmentHasHumidity();
bool environmentHasPressure();
bool environmentHasCo2();
bool environmentHasIlluminance();
bool environmentHasVocIndex();
bool environmentHasGasResistance();
