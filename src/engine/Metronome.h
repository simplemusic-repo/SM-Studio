// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

namespace sms
{

/** Synthesised click. Real-time safe (no allocation, no locks). */
class Metronome
{
public:
    void prepare (double sampleRate) noexcept;

    /** Adds clicks for the block [startSample, startSample + numSamples) to the
        output. samplesPerBeat / beatsPerBar describe the current tempo. */
    void process (float* const* outputs, int numChannels, int numSamples,
                  std::int64_t startSample, double samplesPerBeat, int beatsPerBar,
                  float gain) noexcept;

    void reset() noexcept { remaining = 0; }

private:
    void trigger (bool accent) noexcept;

    double sr = 44100.0;
    double phase = 0.0, phaseInc = 0.0;
    float env = 0.0f, envMul = 0.0f, amp = 0.0f;
    int remaining = 0;
};

} // namespace sms
