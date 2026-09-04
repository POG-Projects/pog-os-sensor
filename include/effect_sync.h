#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace pogdev {

constexpr uint16_t kDefaultPresentationDelayMs = 0;
constexpr uint32_t kEffectFrameTimeoutMs = 500;
constexpr size_t kEffectFrameCapacity = 8;

enum class EffectVisualizer : uint8_t {
  Spectrum,
  VuMeter,
  BassPulse,
  Rainbow,
};

struct EffectPixel {
  float hue = 0;
  float saturation = 1;
  float value = 0;
};

struct EffectFrame {
  uint32_t seq = 0;
  uint64_t monoMs = 0;
  uint64_t presentAtMs = 0;
  uint16_t leadMs = 0;
  float level = 0;
  float bass = 0;
  float treble = 0;
  uint32_t receivedMs = 0;
};

struct EffectSync {
  bool joined = false;
  bool hasPrevious = false;
  bool hasLatest = false;
  char groupId[37] = {};
  char leaderEntityId[37] = {};
  uint16_t presentationDelayMs = kDefaultPresentationDelayMs;
  int16_t calibrationOffsetMs = 0;
  EffectVisualizer visualizer = EffectVisualizer::Spectrum;
  EffectFrame frames[kEffectFrameCapacity];
  size_t frameCount = 0;
  bool hasRendered = false;
  uint32_t lastRenderedSeq = 0;
};

inline bool effectVisualizerFromName(const char *value,
                                     EffectVisualizer &out) {
  if (value == nullptr || std::strcmp(value, "spectrum") == 0) {
    out = EffectVisualizer::Spectrum;
    return true;
  }
  if (std::strcmp(value, "vu_meter") == 0) {
    out = EffectVisualizer::VuMeter;
    return true;
  }
  if (std::strcmp(value, "bass_pulse") == 0) {
    out = EffectVisualizer::BassPulse;
    return true;
  }
  if (std::strcmp(value, "rainbow") == 0) {
    out = EffectVisualizer::Rainbow;
    return true;
  }
  return false;
}

inline float effectPixelPosition(size_t index, size_t count) {
  return count ? (static_cast<float>(index) + 0.5f) /
                     static_cast<float>(count)
               : 0.0f;
}

inline EffectPixel effectVisualizerPixel(EffectVisualizer visualizer,
                                         float position,
                                         const EffectFrame &frame) {
  if (position < 0) position = 0;
  if (position > 1) position = 1;
  switch (visualizer) {
    case EffectVisualizer::Spectrum:
      return {position * (2.0f / 3.0f), 1.0f,
              frame.bass + (frame.treble - frame.bass) * position};
    case EffectVisualizer::VuMeter:
      return {(1.0f - position) / 3.0f, 1.0f,
              position <= frame.level ? 1.0f : 0.0f};
    case EffectVisualizer::BassPulse:
      return {1.0f / 12.0f, 1.0f, frame.bass};
    case EffectVisualizer::Rainbow: {
      float hue = position +
                  static_cast<float>(frame.monoMs % 8000ULL) / 8000.0f;
      if (hue >= 1.0f) hue -= 1.0f;
      return {hue, 1.0f, frame.level};
    }
  }
  return {};
}

inline bool canonicalUuid(const char *value) {
  if (!value || std::strlen(value) != 36) return false;
  for (size_t i = 0; i < 36; ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (value[i] != '-') return false;
    } else if (!((value[i] >= '0' && value[i] <= '9') ||
                 (value[i] >= 'a' && value[i] <= 'f') ||
                 (value[i] >= 'A' && value[i] <= 'F'))) {
      return false;
    }
  }
  return true;
}

inline bool effectSyncJoin(EffectSync &state, const char *groupId,
                           const char *role, const char *leaderEntityId,
                           uint16_t presentationDelayMs =
                               kDefaultPresentationDelayMs,
                           int16_t calibrationOffsetMs = 0,
                           const char *visualizer = "spectrum") {
  EffectVisualizer parsedVisualizer;
  if (!canonicalUuid(groupId) || !canonicalUuid(leaderEntityId) || !role ||
      std::strcmp(role, "follower") != 0 || presentationDelayMs > 500 ||
      calibrationOffsetMs < -100 ||
      calibrationOffsetMs > 100 ||
      !effectVisualizerFromName(visualizer, parsedVisualizer)) {
    return false;
  }
  state = EffectSync{};
  std::memcpy(state.groupId, groupId, 37);
  std::memcpy(state.leaderEntityId, leaderEntityId, 37);
  state.joined = true;
  state.presentationDelayMs = presentationDelayMs;
  state.calibrationOffsetMs = calibrationOffsetMs;
  state.visualizer = parsedVisualizer;
  return true;
}

inline bool effectSyncLeave(EffectSync &state, const char *groupId) {
  if (!state.joined || !canonicalUuid(groupId) ||
      std::strcmp(state.groupId, groupId) != 0) {
    return false;
  }
  state = EffectSync{};
  return true;
}

inline bool seqAfter(uint32_t candidate, uint32_t reference) {
  return static_cast<int32_t>(candidate - reference) > 0;
}

inline bool effectSyncPush(EffectSync &state, uint32_t seq, uint64_t monoMs,
                           uint64_t presentAtMs, uint16_t leadMs,
                           float level, float bass, float treble,
                           uint32_t receivedMs) {
  if (!state.joined || !std::isfinite(level) || !std::isfinite(bass) ||
      !std::isfinite(treble) || level < 0 || level > 1 || bass < 0 ||
      bass > 1 || treble < 0 || treble > 1) {
    return false;
  }
  if (state.hasRendered && !seqAfter(seq, state.lastRenderedSeq)) return false;
  size_t at = 0;
  while (at < state.frameCount && seqAfter(seq, state.frames[at].seq)) ++at;
  if (at < state.frameCount && state.frames[at].seq == seq) return false;
  if (state.frameCount == kEffectFrameCapacity) {
    if (at == 0) return false;
    std::memmove(state.frames, state.frames + 1,
                 (kEffectFrameCapacity - 1) * sizeof(EffectFrame));
    --state.frameCount;
    --at;
  }
  std::memmove(state.frames + at + 1, state.frames + at,
               (state.frameCount - at) * sizeof(EffectFrame));
  state.frames[at] = {seq, monoMs, presentAtMs, leadMs, level, bass, treble,
                      receivedMs};
  ++state.frameCount;
  return true;
}

inline bool effectSyncSample(EffectSync &state, uint32_t nowMs,
                             uint64_t utcNowMs, EffectFrame &out) {
  if (!state.joined || !state.frameCount ||
      static_cast<uint32_t>(nowMs -
                            state.frames[state.frameCount - 1].receivedMs) >
          kEffectFrameTimeoutMs) {
    return false;
  }
  const bool utc = utcNowMs > 1700000000000ULL && state.frames[0].presentAtMs;
  const int64_t target = (utc ? static_cast<int64_t>(utcNowMs)
                              : static_cast<int64_t>(nowMs)) -
                         state.calibrationOffsetMs;
  auto timeOf = [utc](const EffectFrame &frame) -> int64_t {
    return utc ? static_cast<int64_t>(frame.presentAtMs)
               : static_cast<int64_t>(frame.receivedMs) + frame.leadMs;
  };
  if (target < timeOf(state.frames[0])) return false;
  size_t before = 0;
  while (before + 1 < state.frameCount &&
         timeOf(state.frames[before + 1]) <= target) ++before;
  out = state.frames[before];
  if (before + 1 < state.frameCount) {
    const int64_t start = timeOf(state.frames[before]);
    const int64_t end = timeOf(state.frames[before + 1]);
    if (end > start) {
      const float mix = static_cast<float>(target - start) /
                        static_cast<float>(end - start);
      out.level += (state.frames[before + 1].level - out.level) * mix;
      out.bass += (state.frames[before + 1].bass - out.bass) * mix;
      out.treble += (state.frames[before + 1].treble - out.treble) * mix;
    }
  }
  state.hasRendered = true;
  state.lastRenderedSeq = state.frames[before].seq;
  if (before > 0) {
    std::memmove(state.frames, state.frames + before,
                 (state.frameCount - before) * sizeof(EffectFrame));
    state.frameCount -= before;
  }
  return true;
}

}  // namespace pogdev
