#pragma once

#include <Arduino.h>

enum class RadarKind { None, Ld2410, Ld2450 };

struct RadarTarget {
  bool active = false;
  int16_t xMm = 0;
  int16_t yMm = 0;
  int16_t speedCmS = 0;
  uint16_t resolutionMm = 0;
};

struct RadarReading {
  bool online = false;
  bool occupied = false;
  bool motion = false;
  uint8_t targetCount = 0;
  uint16_t movingDistanceCm = 0;
  uint16_t stationaryDistanceCm = 0;
  uint16_t detectionDistanceCm = 0;
  RadarTarget targets[3];
};

void radarSensorBegin();
bool radarSensorLoop();
const RadarReading &radarSensorReading();
bool radarSensorPresent();
const char *radarSensorModel();
bool radarSensorMateriallyChanged(const RadarReading &previous,
                                  const RadarReading &next);
