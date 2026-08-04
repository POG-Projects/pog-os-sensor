#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

void presenceLightBegin();
bool presenceLightUpdate(bool radarOnline, bool occupied, uint32_t now);
bool presenceLightIsOn();
bool presenceLightAutomatic();
uint8_t presenceLightBrightness();
uint32_t presenceLightColor();
uint16_t presenceLightHoldSeconds();

void presenceLightTurnOn();
void presenceLightTurnOff();
void presenceLightToggle();
void presenceLightSetAutomatic(bool enabled);
void presenceLightSetBrightness(float percent);
void presenceLightSetHs(float hue, float saturation);
void presenceLightSetColorTemperature(float kelvin);
void presenceLightSetHoldSeconds(float seconds);

void presenceLightFillState(JsonObject light, JsonObject automatic,
                            JsonObject hold);
