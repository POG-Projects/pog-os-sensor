#pragma once

#include <Arduino.h>

#include "sampling_policy.h"
#include "radar_sensor.h"

void pogdevBegin();
void pogdevLoop();
void pogdevSetReading(const pogsensor::Reading &reading, bool forcePublish);
void pogdevSetRadar(const RadarReading &reading, bool forcePublish);
void pogdevRefreshManifest();
void pogdevRefreshState();
const String &pogdevHardwareId();
bool pogdevIsAdopted();
bool pogdevIsConnected();
