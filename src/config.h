#pragma once

#include <Arduino.h>

#ifndef POGSENSOR_DEFAULT_SDA
#define POGSENSOR_DEFAULT_SDA 21
#endif

#ifndef POGSENSOR_DEFAULT_SCL
#define POGSENSOR_DEFAULT_SCL 22
#endif
#ifndef POGSENSOR_RADAR_A_RX
#define POGSENSOR_RADAR_A_RX 3
#endif
#ifndef POGSENSOR_RADAR_A_TX
#define POGSENSOR_RADAR_A_TX 4
#endif
#ifndef POGSENSOR_RADAR_B_RX
#define POGSENSOR_RADAR_B_RX 20
#endif
#ifndef POGSENSOR_RADAR_B_TX
#define POGSENSOR_RADAR_B_TX 21
#endif
#ifndef POGSENSOR_STATUS_LED_PIN
#define POGSENSOR_STATUS_LED_PIN 7
#endif
#ifndef POGSENSOR_STATUS_LED_COUNT
#define POGSENSOR_STATUS_LED_COUNT 4
#endif

struct DeviceConfig {
  String wifiSsid;
  String wifiPassword;
  String name = "Capteur POG";
  String pogHomeHost;
  uint16_t pogHomePort = 8090;
  uint8_t sdaPin = POGSENSOR_DEFAULT_SDA;
  uint8_t sclPin = POGSENSOR_DEFAULT_SCL;
  uint8_t radarARxPin = POGSENSOR_RADAR_A_RX;
  uint8_t radarATxPin = POGSENSOR_RADAR_A_TX;
  uint8_t radarBRxPin = POGSENSOR_RADAR_B_RX;
  uint8_t radarBTxPin = POGSENSOR_RADAR_B_TX;
  bool statusLightInstalled = false;
  bool presenceLightAuto = true;
  uint8_t presenceLightBrightness = 55;
  uint32_t presenceLightColor = 0xFFD28A;
  uint16_t presenceLightHoldSeconds = 8;
  uint32_t samplePeriodSeconds = 30;
  float temperatureOffset = 0.0f;
};

extern DeviceConfig g_config;

void configBegin();
bool configLoad();
bool configSave();
