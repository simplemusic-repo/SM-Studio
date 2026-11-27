// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"

#include <cmath>

namespace sms
{

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdownAudio();
}

juce::String AudioEngine::initialiseAudio()
{
    auto error = deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceManager.addAudioCallback (this);
    return error;
}

void AudioEngine::shutdownAudio()
{
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

juce::String AudioEngine::getDeviceName() const
{
    if (auto* d = deviceManager.getCurrentAudioDevice())
        return d->getName();
    return {};
}

void AudioEngine::setPositionSeconds (double seconds) noexcept
{
    positionSamples.store ((std::int64_t) std::llround (std::max (0.0, seconds) * sampleRate.load()));
}

double AudioEngine::getPositionSeconds() const noexcept
{
    return (double) positionSamples.load() / sampleRate.load();
}

void AudioEngine::setTempo (double newBpm, TimeSignature sig) noexcept
{
    bpm.store (newBpm);
    beatsPerBar.store (sig.numerator);
    beatUnit.store (sig.denominator);
}

void AudioEngine::setMasterGainDb (float db) noexcept
{
    masterGain.store (juce::Decibels::decibelsToGain (db));
}

float AudioEngine::getAndResetPeak (int channel) noexcept
{
    return peaks[juce::jlimit (0, 1, channel)].exchange (0.0f);
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    // keep the musical position when the sample rate changes
    const auto seconds = getPositionSeconds();
    sampleRate.store (device->getCurrentSampleRate());
    bufferSize.store (device->getCurrentBufferSizeSamples());
    setPositionSeconds (seconds);
    metronome.prepare (device->getCurrentSampleRate());
}

void AudioEngine::audioDeviceStopped()
{
    bufferSize.store (0);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputs, int numOutputs, int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputs[ch], numSamples);

    const bool isNowPlaying = playing.load();

    if (isNowPlaying != wasPlaying)
    {
        metronome.reset();
        wasPlaying = isNowPlaying;
    }

    if (isNowPlaying)
    {
        const auto start = positionSamples.load();

        // --- song rendering goes here (stage 2) ---

        if (metronomeOn.load())
        {
            const auto sr = sampleRate.load();
            const auto samplesPerBeat = sr * 60.0 / bpm.load() * 4.0 / (double) beatUnit.load();
            metronome.process (outputs, numOutputs, numSamples, start, samplesPerBeat, beatsPerBar.load(), 1.0f);
        }

        // only advance if nobody relocated the playhead during this block
        auto expected = start;
        positionSamples.compare_exchange_strong (expected, start + numSamples);
    }

    const auto gain = masterGain.load();

    for (int ch = 0; ch < numOutputs; ++ch)
    {
        if (outputs[ch] == nullptr)
            continue;

        if (! juce::exactlyEqual (gain, 1.0f))
            juce::FloatVectorOperations::multiply (outputs[ch], gain, numSamples);

        if (ch < 2)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (outputs[ch], numSamples);
            const auto peak = std::max (std::abs (range.getStart()), std::abs (range.getEnd()));
            auto& p = peaks[ch];
            auto current = p.load();
            while (peak > current && ! p.compare_exchange_weak (current, peak)) {}
        }
    }
}

} // namespace sms
