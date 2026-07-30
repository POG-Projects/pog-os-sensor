#pragma once

#include <Arduino.h>

#include "sampling_policy.h"

void pogdevBegin();
void pogdevLoop();
void pogdevSetReading(const pogsensor::Reading &reading, bool forcePublish);
const String &pogdevHardwareId();
bool pogdevIsAdopted();
bool pogdevIsConnected();
