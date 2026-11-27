// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Metronome.h"

#include <cmath>

namespace sms
{

void Metronome::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    reset();
}

void Metronome::trigger (bool accent) noexcept
{
    constexpr double twoPi = 6.283185307179586;
    phase = 0.0;
    phaseInc = twoPi * (accent ? 1760.0 : 1320.0) / sr;
    amp = accent ? 0.5f : 0.32f;
    env = 1.0f;
    const double lengthSeconds = 0.035;
    remaining = (int) (sr * lengthSeconds);
    envMul = (float) std::pow (0.001, 1.0 / (double) remaining); // -60 dB over the click
}

void Metronome::process (float* const* outputs, int numChannels, int numSamples,
                         std::int64_t startSample, double samplesPerBeat, int beatsPerBar,
                         float gain) noexcept
{
    if (samplesPerBeat <= 1.0 || beatsPerBar <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto pos = startSample + i;

        // a beat starts in this sample if the beat index differs from the previous sample's
        const auto beat = (std::int64_t) std::floor ((double) pos / samplesPerBeat);
        const auto prevBeat = pos <= 0 ? (std::int64_t) -1
                                       : (std::int64_t) std::floor ((double) (pos - 1) / samplesPerBeat);

        if (pos >= 0 && beat != prevBeat)
            trigger (beat % beatsPerBar == 0);

        if (remaining > 0)
        {
            const auto s = (float) std::sin (phase) * env * amp * gain;
            phase += phaseInc;
            env *= envMul;
            --remaining;

            for (int ch = 0; ch < numChannels; ++ch)
                if (outputs[ch] != nullptr)
                    outputs[ch][i] += s;
        }
    }
}

} // namespace sms
