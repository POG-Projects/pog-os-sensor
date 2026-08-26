#include "radar_sensor.h"

#include <HardwareSerial.h>

#include "config.h"
#include "radar_protocol.h"

#ifndef POGSENSOR_RADAR_UART_B
#define POGSENSOR_RADAR_UART_B 0
#endif
#ifndef POGSENSOR_RADAR_FULL_SCAN
#define POGSENSOR_RADAR_FULL_SCAN 0
#endif
#ifndef POGSENSOR_RADAR_ACTIVE_PROBE
#define POGSENSOR_RADAR_ACTIVE_PROBE 0
#endif
#ifndef POGSENSOR_RADAR_RX_PULLUP
#define POGSENSOR_RADAR_RX_PULLUP 0
#endif
#ifndef POGSENSOR_RADAR_RX_INVERTED
#define POGSENSOR_RADAR_RX_INVERTED 0
#endif

namespace {
constexpr uint32_t kRadarBaud = 256000;
constexpr uint32_t kAlternateRadarBauds[] = {230400, 115200, 57600,
                                              38400,  19200,  9600};
constexpr uint32_t kOfflineAfterMs = 3000;
constexpr uint32_t kWiringProbeMs = 450;
constexpr size_t kRawSampleSize = 16;
HardwareSerial radarA(1);
HardwareSerial radarB(POGSENSOR_RADAR_UART_B);

struct PortParser {
  HardwareSerial *serial;
  uint8_t data[96] = {};
  size_t size = 0;
  RadarKind kind = RadarKind::None;
  uint32_t lastFrame = 0;
  uint32_t rawBytes = 0;
  uint8_t rawSample[kRawSampleSize] = {};
  size_t rawSampleSize = 0;
};

struct ProbeStats {
  uint32_t rawBytes = 0;
  uint8_t sample[kRawSampleSize] = {};
  size_t sampleSize = 0;
};

PortParser ports[] = {{&radarA}, {&radarB}};
RadarReading reading;
RadarReading readings[2];
String model = "aucun radar UART";
pogsensor::radar::WiringStatus wiring[2] = {
    pogsensor::radar::WiringStatus::Unknown,
    pogsensor::radar::WiringStatus::Unknown};
uint32_t activeBauds[2] = {kRadarBaud, kRadarBaud};

void consume(PortParser &port, size_t count) {
  if (count >= port.size) {
    port.size = 0;
    return;
  }
  memmove(port.data, port.data + count, port.size - count);
  port.size -= count;
}

bool parseLd2450(PortParser &port, RadarReading &out) {
  if (port.size < 30) return false;
  for (size_t i = 0; i + 30 <= port.size; ++i) {
    if (port.data[i] != 0xAA || port.data[i + 1] != 0xFF ||
        port.data[i + 2] != 0x03 || port.data[i + 3] != 0x00 ||
        port.data[i + 28] != 0x55 || port.data[i + 29] != 0xCC) {
      continue;
    }
    RadarReading next;
    next.online = true;
    pogsensor::radar::Target decoded[3];
    if (!pogsensor::radar::parseLd2450(port.data + i, 30, decoded,
                                       next.targetCount)) continue;
    for (size_t target = 0; target < 3; ++target) {
      next.targets[target].active = decoded[target].active;
      next.targets[target].xMm = decoded[target].xMm;
      next.targets[target].yMm = decoded[target].yMm;
      next.targets[target].speedCmS = decoded[target].speedCmS;
      next.targets[target].resolutionMm = decoded[target].resolutionMm;
      next.motion |= decoded[target].active && decoded[target].speedCmS != 0;
    }
    next.occupied = next.targetCount > 0;
    out = next;
    port.kind = RadarKind::Ld2450;
    port.lastFrame = millis();
    consume(port, i + 30);
    return true;
  }
  return false;
}

bool parseLd2410(PortParser &port, RadarReading &out) {
  if (port.size < 10) return false;
  for (size_t i = 0; i + 10 <= port.size; ++i) {
    if (port.data[i] != 0xF4 || port.data[i + 1] != 0xF3 ||
        port.data[i + 2] != 0xF2 || port.data[i + 3] != 0xF1) {
      continue;
    }
    uint16_t payloadSize = pogsensor::radar::littleU16(port.data + i + 4);
    size_t frameSize = 4 + 2 + payloadSize + 4;
    if (payloadSize < 13 || frameSize > sizeof(port.data)) {
      consume(port, i + 4);
      return false;
    }
    if (i + frameSize > port.size) return false;
    size_t footer = i + frameSize - 4;
    if (port.data[footer] != 0xF8 || port.data[footer + 1] != 0xF7 ||
        port.data[footer + 2] != 0xF6 || port.data[footer + 3] != 0xF5) {
      consume(port, i + 4);
      return false;
    }
    pogsensor::radar::Presence presence;
    if (!pogsensor::radar::parseLd2410(port.data + i, frameSize, presence)) {
      consume(port, i + frameSize);
      return false;
    }
    RadarReading next;
    next.online = true;
    next.motion = presence.motion;
    next.occupied = presence.occupied;
    next.targetCount = next.occupied ? 1 : 0;
    next.movingDistanceCm = presence.movingDistanceCm;
    next.stationaryDistanceCm = presence.stationaryDistanceCm;
    next.detectionDistanceCm = presence.detectionDistanceCm;
    out = next;
    port.kind = RadarKind::Ld2410;
    port.lastFrame = millis();
    consume(port, i + frameSize);
    return true;
  }
  return false;
}

bool poll(PortParser &port, RadarReading &out) {
  while (port.serial->available()) {
    if (port.size == sizeof(port.data)) consume(port, 1);
    uint8_t value = port.serial->read();
    port.data[port.size++] = value;
    ++port.rawBytes;
    if (port.rawSampleSize < sizeof(port.rawSample)) {
      port.rawSample[port.rawSampleSize++] = value;
    }
  }
  bool changed = false;
  while (parseLd2450(port, out) || parseLd2410(port, out)) changed = true;
  if (!changed && port.size > 64) consume(port, port.size - 64);
  if (out.online && millis() - port.lastFrame > kOfflineAfterMs) {
    out = RadarReading{};
    changed = true;
  }
  return changed;
}

void rebuildCombined() {
  RadarReading combined;
  uint8_t spatialTargets = 0;
  uint8_t nonSpatialTargets = 0;
  for (const RadarReading &source : readings) {
    if (!source.online) continue;
    combined.online = true;
    combined.occupied |= source.occupied;
    combined.motion |= source.motion;
    combined.movingDistanceCm =
        max(combined.movingDistanceCm, source.movingDistanceCm);
    combined.stationaryDistanceCm =
        max(combined.stationaryDistanceCm, source.stationaryDistanceCm);
    combined.detectionDistanceCm =
        max(combined.detectionDistanceCm, source.detectionDistanceCm);
    bool sourceHasSpatial = source.targets[0].active ||
                            source.targets[1].active ||
                            source.targets[2].active;
    if (!sourceHasSpatial) {
      nonSpatialTargets = max(nonSpatialTargets, source.targetCount);
    }
    for (const RadarTarget &target : source.targets) {
      if (!target.active || spatialTargets >= 3) continue;
      combined.targets[spatialTargets++] = target;
    }
  }
  combined.targetCount = spatialTargets ? spatialTargets : nonSpatialTargets;
  reading = combined;

  model = "";
  for (const PortParser &port : ports) {
    const char *name = port.kind == RadarKind::Ld2410 ? "LD2410B" :
                       port.kind == RadarKind::Ld2450 ? "LD2450" : nullptr;
    if (!name) continue;
    if (model.indexOf(name) >= 0) continue;
    if (model.length()) model += " + ";
    model += name;
  }
  if (!model.length()) model = "aucun radar UART";
}

void preparePins(uint8_t rxPin, uint8_t txPin) {
  pinMode(rxPin, INPUT);
  pinMode(txPin, INPUT);
}

void beginReceiveOnly(size_t index, uint8_t pin, uint8_t configuredRx,
                      uint8_t configuredTx, uint32_t baud = kRadarBaud) {
  PortParser &port = ports[index];
  port.serial->end();
  preparePins(configuredRx, configuredTx);
  port.size = 0;
  port.kind = RadarKind::None;
  port.lastFrame = 0;
  port.rawBytes = 0;
  port.rawSampleSize = 0;
  if (POGSENSOR_RADAR_RX_PULLUP) pinMode(pin, INPUT_PULLUP);
  port.serial->begin(baud, SERIAL_8N1, pin, -1,
                     POGSENSOR_RADAR_RX_INVERTED != 0);
}

ProbeStats probeStats(const PortParser &port) {
  ProbeStats stats;
  stats.rawBytes = port.rawBytes;
  stats.sampleSize = port.rawSampleSize;
  memcpy(stats.sample, port.rawSample, stats.sampleSize);
  return stats;
}

void printProbeStats(size_t index, const char *side, uint8_t pin,
                     uint32_t baud, const ProbeStats &stats) {
  Serial.printf("[RadarDiag] port %c %s GPIO %u @ %lu: %lu octet(s)",
                static_cast<char>('A' + index), side, pin,
                static_cast<unsigned long>(baud),
                static_cast<unsigned long>(stats.rawBytes));
  if (stats.sampleSize) {
    Serial.print(" · ");
    for (size_t offset = 0; offset < stats.sampleSize; ++offset) {
      if (offset) Serial.print(' ');
      if (stats.sample[offset] < 0x10) Serial.print('0');
      Serial.print(stats.sample[offset], HEX);
    }
  }
  Serial.println();
}

void probePorts(const bool enabled[2]) {
  uint32_t until = millis() + kWiringProbeMs;
  while ((int32_t)(millis() - until) < 0) {
    for (size_t index = 0; index < 2; ++index) {
      if (enabled[index]) poll(ports[index], readings[index]);
    }
    delay(2);
  }
}

bool probeOtherBauds(size_t index, const char *side, uint8_t pin,
                     uint8_t configuredRx, uint8_t configuredTx,
                     RadarKind &detectedKind, uint32_t &detectedBaud) {
  const bool enabled[2] = {index == 0, index == 1};
  for (uint32_t baud : kAlternateRadarBauds) {
    readings[index] = RadarReading{};
    beginReceiveOnly(index, pin, configuredRx, configuredTx, baud);
    probePorts(enabled);
    printProbeStats(index, side, pin, baud, probeStats(ports[index]));
    if (ports[index].kind == RadarKind::None) continue;
    detectedKind = ports[index].kind;
    detectedBaud = baud;
    return true;
  }
  return false;
}

#if POGSENSOR_RADAR_ACTIVE_PROBE
void probeLd2410Command(size_t index, uint8_t rxPin, uint8_t txPin) {
  static constexpr uint8_t kEnableConfiguration[] = {
      0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF,
      0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
  static constexpr uint8_t kEndConfiguration[] = {
      0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00,
      0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};

  PortParser &port = ports[index];
  port.serial->end();
  preparePins(rxPin, txPin);
  port.size = 0;
  port.rawBytes = 0;
  port.rawSampleSize = 0;
  port.serial->begin(kRadarBaud, SERIAL_8N1, rxPin, txPin);
  delay(50);
  while (port.serial->available()) port.serial->read();

  port.serial->write(kEnableConfiguration, sizeof(kEnableConfiguration));
  port.serial->flush();
  const bool enabled[2] = {index == 0, index == 1};
  probePorts(enabled);
  printProbeStats(index, "PING RX", rxPin, kRadarBaud, probeStats(port));

  bool acknowledged = false;
  for (size_t offset = 0; offset + 4 <= port.size; ++offset) {
    acknowledged |= port.data[offset] == 0xFD &&
                    port.data[offset + 1] == 0xFC &&
                    port.data[offset + 2] == 0xFB &&
                    port.data[offset + 3] == 0xFA;
  }
  Serial.printf("[RadarDiag] commande LD2410: %s\n",
                acknowledged ? "réponse reçue" : "aucune réponse");
  port.serial->write(kEndConfiguration, sizeof(kEndConfiguration));
  port.serial->flush();
  port.serial->end();
  preparePins(rxPin, txPin);
}
#endif
}  // namespace

void radarSensorBegin() {
  const uint8_t rxPins[2] = {g_config.radarARxPin, g_config.radarBRxPin};
  const uint8_t txPins[2] = {g_config.radarATxPin, g_config.radarBTxPin};
  const bool bothPorts[2] = {true, true};
  for (size_t index = 0; index < 2; ++index) {
    readings[index] = RadarReading{};
    beginReceiveOnly(index, rxPins[index], rxPins[index], txPins[index]);
  }
  probePorts(bothPorts);

  ProbeStats expectedStats[2] = {probeStats(ports[0]), probeStats(ports[1])};

  bool detectedOnExpected[2] = {ports[0].kind != RadarKind::None,
                                ports[1].kind != RadarKind::None};
  bool probeAlternate[2] = {!detectedOnExpected[0], !detectedOnExpected[1]};
  for (size_t index = 0; index < 2; ++index) {
    if (!probeAlternate[index]) continue;
    readings[index] = RadarReading{};
    beginReceiveOnly(index, txPins[index], rxPins[index], txPins[index]);
  }
  probePorts(probeAlternate);

  ProbeStats alternateStats[2];
  for (size_t index = 0; index < 2; ++index) {
    if (probeAlternate[index]) alternateStats[index] = probeStats(ports[index]);
    printProbeStats(index, "RX", rxPins[index], kRadarBaud,
                    expectedStats[index]);
    if (probeAlternate[index]) {
      printProbeStats(index, "TX", txPins[index], kRadarBaud,
                      alternateStats[index]);
    }
  }

#if POGSENSOR_RADAR_ACTIVE_PROBE
  if (!detectedOnExpected[0]) {
    probeLd2410Command(0, rxPins[0], txPins[0]);
  }
#endif

  for (size_t index = 0; index < 2; ++index) {
    bool detectedOnAlternate =
        probeAlternate[index] && ports[index].kind != RadarKind::None;
    RadarKind detectedKind = ports[index].kind;
    uint32_t detectedBaud = kRadarBaud;

    if (!detectedOnExpected[index] && !detectedOnAlternate) {
      if (POGSENSOR_RADAR_FULL_SCAN || expectedStats[index].rawBytes) {
        detectedOnExpected[index] = probeOtherBauds(
            index, "RX", rxPins[index], rxPins[index], txPins[index],
            detectedKind, detectedBaud);
      }
      if (!detectedOnExpected[index] &&
          (POGSENSOR_RADAR_FULL_SCAN || alternateStats[index].rawBytes)) {
        detectedOnAlternate = probeOtherBauds(
            index, "TX", txPins[index], rxPins[index], txPins[index],
            detectedKind, detectedBaud);
      }
    }

    wiring[index] = pogsensor::radar::classifyWiring(
        detectedOnExpected[index], detectedOnAlternate);
    uint8_t activeRx = detectedOnAlternate ? txPins[index] : rxPins[index];
    activeBauds[index] = detectedBaud;
    beginReceiveOnly(index, activeRx, rxPins[index], txPins[index],
                     activeBauds[index]);
    ports[index].kind = detectedKind;
    ports[index].lastFrame = detectedKind == RadarKind::None ? 0 : millis();
  }
  rebuildCombined();
}

bool radarSensorLoop() {
  RadarReading previous = reading;
  bool received = poll(ports[0], readings[0]);
  received |= poll(ports[1], readings[1]);
  if (received) rebuildCombined();
  return radarSensorMateriallyChanged(previous, reading);
}

const RadarReading &radarSensorReading() { return reading; }
bool radarSensorPresent() {
  return ports[0].kind != RadarKind::None || ports[1].kind != RadarKind::None;
}
const char *radarSensorModel() { return model.c_str(); }

const char *radarSensorPortModel(size_t index) {
  if (index >= 2) return "inconnu";
  if (ports[index].kind == RadarKind::Ld2410) return "LD2410B";
  if (ports[index].kind == RadarKind::Ld2450) return "LD2450";
  return "aucun radar";
}

pogsensor::radar::WiringStatus radarSensorPortWiring(size_t index) {
  return index < 2 ? wiring[index] : pogsensor::radar::WiringStatus::Unknown;
}

uint32_t radarSensorPortBaud(size_t index) {
  return index < 2 ? activeBauds[index] : 0;
}

const char *radarSensorWiringName(pogsensor::radar::WiringStatus status) {
  switch (status) {
    case pogsensor::radar::WiringStatus::Correct:
      return "correct";
    case pogsensor::radar::WiringStatus::Reversed:
      return "RX/TX inversés (réception corrigée en logiciel)";
    case pogsensor::radar::WiringStatus::NoSignal:
      return "aucun signal valide";
    default:
      return "non testé";
  }
}

bool radarSensorMateriallyChanged(const RadarReading &previous,
                                  const RadarReading &next) {
  if (previous.online != next.online || previous.occupied != next.occupied ||
      previous.motion != next.motion ||
      previous.targetCount != next.targetCount) return true;
  if (abs(int(previous.detectionDistanceCm) -
          int(next.detectionDistanceCm)) >= 10) return true;
  if (abs(int(previous.movingDistanceCm) -
          int(next.movingDistanceCm)) >= 10 ||
      abs(int(previous.stationaryDistanceCm) -
          int(next.stationaryDistanceCm)) >= 10) return true;
  for (size_t i = 0; i < 3; ++i) {
    const RadarTarget &a = previous.targets[i];
    const RadarTarget &b = next.targets[i];
    if (a.active != b.active) return true;
    if (!b.active) continue;
    if (abs(int(a.xMm) - int(b.xMm)) >= 100 ||
        abs(int(a.yMm) - int(b.yMm)) >= 100 ||
        abs(int(a.speedCmS) - int(b.speedCmS)) >= 10) return true;
  }
  return false;
}
