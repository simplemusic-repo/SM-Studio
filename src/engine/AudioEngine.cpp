// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"

#include <cmath>

namespace sms
{

namespace
{
    constexpr int fadeSamples = 64; // de-click fades at clip boundaries
}

AudioEngine::AudioEngine()
{
    lastGains.resize (maxTracks, { 0.0f, 0.0f });
    lastGainIds.resize (maxTracks);
    prepareToPlay (44100.0, 512);
}

AudioEngine::~AudioEngine()
{
    shutdownAudio();
    current.store (nullptr);
    inUse.store (nullptr);
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

void AudioEngine::prepareToPlay (double newSampleRate, int maxBlockSize)
{
    // keep the musical position when the sample rate changes
    const auto seconds = getPositionSeconds();
    sampleRate.store (newSampleRate);
    setPositionSeconds (seconds);
    scratch.setSize (2, std::max (maxBlockSize, 1024));
    metronome.prepare (newSampleRate);
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

void AudioEngine::setLoop (double start, double end, bool enabled) noexcept
{
    loopStartSeconds.store (start);
    loopEndSeconds.store (end);
    loopOn.store (enabled && end > start + 0.01);
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
void AudioEngine::publish (std::unique_ptr<RenderSnapshot> s)
{
    if (s == nullptr)
        return;

    current.store (s.get());
    owned.push_back (std::move (s));
    collectGarbage();
}

void AudioEngine::collectGarbage()
{
    auto* cur = current.load();
    auto* used = inUse.load();

    owned.erase (std::remove_if (owned.begin(), owned.end(),
                                 [cur, used] (auto& p) { return p.get() != cur && p.get() != used; }),
                 owned.end());
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    bufferSize.store (device->getCurrentBufferSizeSamples());
    prepareToPlay (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    bufferSize.store (0);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputs, int numOutputs, int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    renderBlock (outputs, numOutputs, numSamples);
}

void AudioEngine::renderBlock (float* const* outputs, int numOutputs, int numSamples) noexcept
{
    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputs[ch], numSamples);

    if (numOutputs <= 0 || outputs[0] == nullptr)
        return;

    float* left = outputs[0];
    float* right = numOutputs > 1 ? outputs[1] : nullptr;

    // announce which snapshot we use (hazard pointer, see collectGarbage)
    RenderSnapshot* snap = nullptr;
    do
    {
        snap = current.load();
        inUse.store (snap);
    } while (snap != current.load());

    const bool isNowPlaying = playing.load();
    if (isNowPlaying != wasPlaying)
    {
        metronome.reset();
        wasPlaying = isNowPlaying;
    }

    if (isNowPlaying)
    {
        const auto sr = sampleRate.load();
        const auto startPos = positionSamples.load();
        auto pos = startPos;

        const bool looping = loopOn.load();
        const auto loopStart = (std::int64_t) std::llround (loopStartSeconds.load() * sr);
        const auto loopEnd = (std::int64_t) std::llround (loopEndSeconds.load() * sr);

        const auto samplesPerBeat = sr * 60.0 / bpm.load() * 4.0 / (double) beatUnit.load();
        int done = 0;

        while (done < numSamples)
        {
            int n = std::min (numSamples - done, scratch.getNumSamples());

            if (looping && pos < loopEnd && pos + n > loopEnd)
                n = (int) (loopEnd - pos);

            float* segL = left + done;
            float* segR = right != nullptr ? right + done : nullptr;

            if (snap != nullptr)
                renderSegment (*snap, segL, segR, pos, n);

            if (metronomeOn.load())
            {
                float* chans[] = { segL, segR };
                metronome.process (chans, segR != nullptr ? 2 : 1, n, pos, samplesPerBeat, beatsPerBar.load(), 1.0f);
            }

            pos += n;
            done += n;

            if (looping && pos >= loopEnd)
            {
                pos = loopStart;
                metronome.reset();
            }
        }

        // only advance if nobody relocated the playhead during this block
        auto expected = startPos;
        positionSamples.compare_exchange_strong (expected, pos);
    }

    inUse.store (nullptr);

    const auto gain = masterGain.load();

    for (int ch = 0; ch < std::min (2, numOutputs); ++ch)
    {
        if (outputs[ch] == nullptr)
            continue;

        if (! juce::exactlyEqual (gain, 1.0f))
            juce::FloatVectorOperations::multiply (outputs[ch], gain, numSamples);

        const auto range = juce::FloatVectorOperations::findMinAndMax (outputs[ch], numSamples);
        const auto peak = std::max (std::abs (range.getStart()), std::abs (range.getEnd()));
        auto& p = peaks[ch];
        auto currentPeak = p.load();
        while (peak > currentPeak && ! p.compare_exchange_weak (currentPeak, peak)) {}
    }
}

void AudioEngine::renderSegment (const RenderSnapshot& snap, float* outL, float* outR, std::int64_t start, int n) noexcept
{
    const auto end = start + n;
    const auto numTracks = std::min ((int) snap.tracks.size(), maxTracks);

    for (int ti = 0; ti < numTracks; ++ti)
    {
        const auto& track = snap.tracks[(size_t) ti];

        // gain ramp from the previous segment's gains (slot reset if another track is here now)
        auto& last = lastGains[(size_t) ti];
        if (lastGainIds[(size_t) ti] != track.id)
        {
            lastGainIds[(size_t) ti] = track.id;
            last = { track.gainL, track.gainR };
        }

        const auto targetL = track.gainL, targetR = track.gainR;
        const bool silent = juce::exactlyEqual (targetL, 0.0f) && juce::exactlyEqual (targetR, 0.0f)
                         && juce::exactlyEqual (last[0], 0.0f) && juce::exactlyEqual (last[1], 0.0f);

        if (silent || track.clips.empty())
        {
            last = { targetL, targetR };
            continue;
        }

        auto* bufL = scratch.getWritePointer (0);
        auto* bufR = scratch.getWritePointer (1);
        juce::FloatVectorOperations::clear (bufL, n);
        juce::FloatVectorOperations::clear (bufR, n);
        bool anything = false;

        for (auto& c : track.clips)
        {
            const auto clipEnd = c.start + c.length;
            if (c.start >= end)
                break; // sorted by start
            if (clipEnd <= start)
                continue;

            const auto from = std::max (start, c.start);
            const auto to = std::min (end, clipEnd);
            const auto& buffer = c.audio->buffer;
            const auto srcLen = buffer.getNumSamples();
            const auto* srcL = buffer.getReadPointer (0);
            const auto* srcR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : srcL;
            anything = true;

            for (auto t = from; t < to; ++t)
            {
                const auto srcPos = c.sourceStart + (double) (t - c.start) * c.ratio;
                const auto idx = (int) srcPos;
                if (idx < 0 || idx >= srcLen)
                    continue;

                const auto frac = (float) (srcPos - (double) idx);
                const auto next = std::min (idx + 1, srcLen - 1);

                auto env = c.gain;
                const auto fromStart = t - c.start, toEnd = clipEnd - t;
                if (fromStart < fadeSamples) env *= (float) (fromStart + 1) / (float) fadeSamples;
                if (toEnd < fadeSamples)     env *= (float) toEnd / (float) fadeSamples;

                const auto i = (int) (t - start);
                bufL[i] += (srcL[idx] + (srcL[next] - srcL[idx]) * frac) * env;
                bufR[i] += (srcR[idx] + (srcR[next] - srcR[idx]) * frac) * env;
            }
        }

        if (anything)
        {
            const auto stepL = (targetL - last[0]) / (float) n;
            const auto stepR = (targetR - last[1]) / (float) n;
            auto gL = last[0], gR = last[1];

            for (int i = 0; i < n; ++i)
            {
                gL += stepL;
                gR += stepR;
                outL[i] += bufL[i] * gL;
                if (outR != nullptr)
                    outR[i] += bufR[i] * gR;
            }
        }

        last = { targetL, targetR };
    }
}

} // namespace sms
