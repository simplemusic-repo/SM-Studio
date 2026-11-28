// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Metronome.h"

#include <algorithm>
#include <cmath>

namespace sms
{

namespace
{
    constexpr double twoPi = 6.283185307179586;
}

const char* Metronome::getSoundName (int s) noexcept
{
    switch (s)
    {
        case woodblock:  return "Holzblock";
        case cowbell:    return "Kuhglocke";
        case clave:      return "Klick (Clave)";
        case electronic: return "Elektronisch";
        default:         return "Klassisch (Piep)";
    }
}

void Metronome::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    reset();
}

void Metronome::trigger (bool accent) noexcept
{
    double freq = 1320.0, freq2 = 0.0, lengthSeconds = 0.035;
    float level = 0.32f;

    switch (sound)
    {
        case woodblock:  freq = accent ? 1250.0 : 900.0;  lengthSeconds = 0.05;  level = 0.45f; break;
        case cowbell:    freq = accent ? 800.0 : 587.0;   freq2 = freq * 1.48; lengthSeconds = 0.12; level = 0.22f; break;
        case clave:      freq = accent ? 2600.0 : 2100.0; lengthSeconds = 0.02;  level = 0.40f; break;
        case electronic: freq = accent ? 1000.0 : 750.0;  lengthSeconds = 0.04;  level = 0.16f; break;
        default:         freq = accent ? 1760.0 : 1320.0; lengthSeconds = 0.035; level = 0.32f; break;
    }

    phase = phase2 = 0.0;
    phaseInc = twoPi * freq / sr;
    phaseInc2 = twoPi * freq2 / sr;
    amp = level * (accent ? 1.55f : 1.0f);
    env = 1.0f;
    noiseLp = 0.0f;
    remaining = std::max (1, (int) (sr * lengthSeconds));
    envMul = (float) std::pow (0.001, 1.0 / (double) remaining); // -60 dB over the click
}

float Metronome::nextSample() noexcept
{
    float s = 0.0f;

    switch (sound)
    {
        case woodblock: // sine with a short noise "knock" at the start
        {
            noise = noise * 1664525u + 1013904223u;
            const auto n = (float) (noise >> 9) / 4194304.0f - 1.0f;
            noiseLp += 0.25f * (n - noiseLp);
            s = (float) std::sin (phase) + noiseLp * env * env * 0.8f;
            break;
        }
        case cowbell: // two square-ish partials
            s = 0.6f * (float) std::tanh (3.0 * std::sin (phase)) + 0.4f * (float) std::tanh (3.0 * std::sin (phase2));
            break;
        case clave:
            s = (float) std::sin (phase) * (env > 0.3f ? 1.0f : env * 3.0f);
            break;
        case electronic: // square wave
            s = std::sin (phase) >= 0.0 ? 1.0f : -1.0f;
            break;
        default:
            s = (float) std::sin (phase);
            break;
    }

    phase += phaseInc;
    phase2 += phaseInc2;
    s *= env * amp;
    env *= envMul;
    --remaining;
    return s;
}

void Metronome::renderTail (float* const* outputs, int numChannels, int numSamples, float gain) noexcept
{
    for (int i = 0; i < numSamples && remaining > 0; ++i)
    {
        const auto s = nextSample() * gain;
        for (int ch = 0; ch < numChannels; ++ch)
            if (outputs[ch] != nullptr)
                outputs[ch][i] += s;
    }
}

void Metronome::process (float* const* outputs, int numChannels, int numSamples,
                         std::int64_t startSample, double samplesPerBeat, int beatsPerBar,
                         float gain, bool accentFirstBeat) noexcept
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
            trigger (accentFirstBeat && beat % beatsPerBar == 0);

        if (remaining > 0)
        {
            const auto s = nextSample() * gain;
            for (int ch = 0; ch < numChannels; ++ch)
                if (outputs[ch] != nullptr)
                    outputs[ch][i] += s;
        }
    }
}

} // namespace sms
