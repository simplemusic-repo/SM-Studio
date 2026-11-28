// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

namespace sms
{

/** Synthesised click with several sounds. Real-time safe (no allocation, no locks). */
class Metronome
{
public:
    enum Sound { classic = 0, woodblock, cowbell, clave, electronic, numSounds };

    /** German display names, index = Sound. */
    static const char* getSoundName (int sound) noexcept;

    void prepare (double sampleRate) noexcept;

    void setSound (int s) noexcept { sound = (s >= 0 && s < numSounds) ? s : classic; }

    /** Adds clicks for the block [startSample, startSample + numSamples) to the
        output. samplesPerBeat / beatsPerBar describe the current tempo.
        accentFirstBeat: beat 1 of each bar sounds higher and louder. */
    void process (float* const* outputs, int numChannels, int numSamples,
                  std::int64_t startSample, double samplesPerBeat, int beatsPerBar,
                  float gain, bool accentFirstBeat = true) noexcept;

    /** Plays one click immediately (preview in the settings). */
    void trigger (bool accent) noexcept;

    /** Renders a sounding click (after trigger) without new beats. */
    void renderTail (float* const* outputs, int numChannels, int numSamples, float gain) noexcept;

    void reset() noexcept { remaining = 0; }

private:
    float nextSample() noexcept;

    double sr = 44100.0;
    int sound = classic;
    double phase = 0.0, phaseInc = 0.0, phase2 = 0.0, phaseInc2 = 0.0;
    float env = 0.0f, envMul = 0.0f, amp = 0.0f;
    float noiseLp = 0.0f;
    std::uint32_t noise = 22222u;
    int remaining = 0;
};

} // namespace sms
