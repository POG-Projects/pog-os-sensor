#pragma once

namespace pogsensor::radar {

enum class WiringStatus { Unknown, Correct, Reversed, NoSignal };

constexpr WiringStatus classifyWiring(bool frameOnExpectedRx,
                                      bool frameOnExpectedTx) {
  if (frameOnExpectedRx) return WiringStatus::Correct;
  if (frameOnExpectedTx) return WiringStatus::Reversed;
  return WiringStatus::NoSignal;
}

}  // namespace pogsensor::radar
