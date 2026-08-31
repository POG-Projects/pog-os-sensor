#include "effect_sync.h"

#include <cassert>
#include <cmath>

int main() {
  constexpr char group[] = "01234567-89ab-cdef-0123-456789abcdef";
  constexpr char leader[] = "fedcba98-7654-3210-fedc-ba9876543210";
  pogdev::EffectSync sync;
  assert(!pogdev::effectSyncJoin(sync, "bad", "follower", leader));
  assert(!pogdev::effectSyncJoin(sync, group, "leader", leader));
  assert(pogdev::effectSyncJoin(sync, group, "follower", leader));
  assert(sync.visualizer == pogdev::EffectVisualizer::Spectrum);
  assert(!pogdev::effectSyncJoin(sync, group, "follower", leader, 0, 0,
                                 "unknown"));
  assert(pogdev::effectSyncJoin(sync, group, "follower", leader, 0, 0,
                                "rainbow"));
  assert(sync.visualizer == pogdev::EffectVisualizer::Rainbow);
  assert(pogdev::effectSyncJoin(sync, group, "follower", leader));
  assert(pogdev::effectSyncPush(sync, 1, 1000, 2000, 40, .2f, .4f, .6f, 100));
  assert(pogdev::effectSyncPush(sync, 2, 1040, 2040, 40, .6f, .8f, 1.0f, 140));
  pogdev::EffectFrame frame;
  assert(pogdev::effectSyncSample(sync, 160, 0, frame));
  assert(std::fabs(frame.bass - .6f) < .001f);
  assert(!pogdev::effectSyncSample(sync, 641, 0, frame));
  assert(pogdev::effectSyncLeave(sync, group));
  assert(std::fabs(pogdev::effectPixelPosition(0, 4) - .125f) < .001f);
  assert(std::fabs(pogdev::effectPixelPosition(3, 4) - .875f) < .001f);
  assert(std::fabs(pogdev::effectPixelPosition(299, 300) -
                   (299.5f / 300.0f)) < .001f);

  frame.monoMs = 2000;
  frame.level = .5f;
  frame.bass = .25f;
  frame.treble = .75f;
  auto pixel = pogdev::effectVisualizerPixel(
      pogdev::EffectVisualizer::Spectrum, .5f, frame);
  assert(std::fabs(pixel.hue - (1.0f / 3.0f)) < .001f);
  assert(std::fabs(pixel.value - .5f) < .001f);
  assert(pogdev::effectVisualizerPixel(pogdev::EffectVisualizer::VuMeter,
                                       .25f, frame).value == 1.0f);
  assert(pogdev::effectVisualizerPixel(pogdev::EffectVisualizer::VuMeter,
                                       .75f, frame).value == 0.0f);
  auto left = pogdev::effectVisualizerPixel(
      pogdev::EffectVisualizer::BassPulse, .1f, frame);
  auto right = pogdev::effectVisualizerPixel(
      pogdev::EffectVisualizer::BassPulse, .9f, frame);
  assert(left.hue == right.hue && left.value == right.value);
  pixel = pogdev::effectVisualizerPixel(pogdev::EffectVisualizer::Rainbow,
                                        .5f, frame);
  assert(std::fabs(pixel.hue - .75f) < .001f);
  assert(std::fabs(pixel.value - .5f) < .001f);
}
