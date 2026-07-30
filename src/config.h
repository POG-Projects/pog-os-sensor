#pragma once

#include <Arduino.h>

#ifndef POGSENSOR_DEFAULT_SDA
#define POGSENSOR_DEFAULT_SDA 21
#endif

#ifndef POGSENSOR_DEFAULT_SCL
#define POGSENSOR_DEFAULT_SCL 22
#endif

struct DeviceConfig {
  String wifiSsid;
  String wifiPassword;
  String name = "Capteur POG";
  String pogHomeHost;
  uint16_t pogHomePort = 8090;
  uint8_t sdaPin = POGSENSOR_DEFAULT_SDA;
  uint8_t sclPin = POGSENSOR_DEFAULT_SCL;
  uint32_t samplePeriodSeconds = 30;
  float temperatureOffset = 0.0f;
};

extern DeviceConfig g_config;

void configBegin();
bool configLoad();
bool configSave();
