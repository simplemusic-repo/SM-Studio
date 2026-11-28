// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"

#include "CrashInfo.h"
#include "PluginHost.h"

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
    activeNotes.resize (maxTracks);
    midi.ensureSize (16384);
    liveBlock.ensureSize (4096);
    prepareToPlay (44100.0, 512);
}

AudioEngine::~AudioEngine()
{
    shutdownAudio();
    current.store (nullptr);
    inUse.store (nullptr);
}

juce::String AudioEngine::initialiseAudio (const juce::XmlElement* savedState)
{
    auto error = deviceManager.initialise (0, 2, savedState, true);

    // first start: listen to every connected MIDI keyboard
    if (savedState == nullptr)
        for (auto& d : juce::MidiInput::getAvailableDevices())
            deviceManager.setMidiInputDeviceEnabled (d.identifier, true);

    deviceManager.addMidiInputDeviceCallback ({}, this); // all enabled inputs
    deviceManager.addAudioCallback (this);
    return error;
}

void AudioEngine::addLiveMidi (const juce::MidiMessage& m)
{
    auto copy = m;
    copy.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    liveCollector.addMessageToQueue (copy);
    liveMidiCount.fetch_add (1);
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    const auto mask = midiThruMask.load();
    const bool pass = (m.isNoteOnOrOff()     && (mask & filterNotes))
                   || (m.isAftertouch()      && (mask & filterPolyPressure))
                   || (m.isController()      && (mask & filterControllers))
                   || (m.isProgramChange()   && (mask & filterProgramChange))
                   || (m.isChannelPressure() && (mask & filterAftertouch))
                   || (m.isPitchWheel()      && (mask & filterPitchbend));
    // SysEx is not forwarded live (MidiMessageCollector is not real-time safe for big messages)
    if (pass)
        addLiveMidi (m);
}

void AudioEngine::shutdownAudio()
{
    deviceManager.removeMidiInputDeviceCallback ({}, this);
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
    maxBlockSize = std::max (16, maxBlockSize);
    maxBlock.store (maxBlockSize);
    scratch.setSize (maxChannels, maxBlockSize);
    metronome.prepare (newSampleRate);
    liveCollector.reset (newSampleRate);
    // plug-ins are NOT prepared here (this may run on the driver's thread):
    // snapshots built for another rate/size are silent until EngineSync has
    // prepared the plug-ins on the message thread and published a new one.
}

void AudioEngine::setPositionSeconds (double seconds) noexcept
{
    notesOffPending.store (true); // a jump: stop sounding notes
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

void AudioEngine::setMetronomeOptions (const MetronomeOptions& o) noexcept
{
    metroSound.store (o.sound);
    metroGain.store (juce::Decibels::decibelsToGain (o.volumeDb, -60.0f));
    metroAccent.store (o.accent);
    metroOnPlay.store (o.clickOnPlay);
    countInBars.store (juce::jlimit (0, 8, o.countInBars));
    countInOnPlay.store (o.countInOnPlay);
    // clickOnRecord / countInOnRecord: used by recording (stage 7)
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

bool AudioEngine::isProcessorInUse (const juce::AudioProcessor* p) const
{
    for (auto* snap : { current.load(), inUse.load() })
    {
        if (snap == nullptr)
            continue;
        for (auto& t : snap->tracks)
        {
            if (t.instrument.get() == p)
                return true;
            for (auto& i : t.inserts)
                if (i.get() == p)
                    return true;
        }
    }
    return false;
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    crashinfo::note ("audio device started: " + device->getTypeName() + " / " + device->getName() + ", "
                     + juce::String (device->getCurrentSampleRate()) + " Hz, " + juce::String (device->getCurrentBufferSizeSamples()) + " samples");
    bufferSize.store (device->getCurrentBufferSizeSamples());
    prepareToPlay (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    crashinfo::note ("audio device stopped");
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
    const juce::ScopedNoDenormals noDenormals; // denormal numbers can cost a lot of CPU

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

    // a snapshot made for another sample rate / block size must not be played
    // (plug-ins would run unprepared): stay silent until the new one arrives
    if (snap != nullptr && (! juce::approximatelyEqual (snap->sampleRate, sampleRate.load()) || snap->maxBlock < scratch.getNumSamples()))
        snap = nullptr;

    const bool isNowPlaying = playing.load();
    metronome.setSound (metroSound.load());
    const auto clickGain = metroGain.load();
    const auto samplesPerBeat = sampleRate.load() * 60.0 / bpm.load() * 4.0 / (double) beatUnit.load();

    const auto generation = playGeneration.load();
    if (isNowPlaying != wasPlaying || (isNowPlaying && generation != seenPlayGeneration))
    {
        metronome.reset();
        wasPlaying = isNowPlaying;
        seenPlayGeneration = generation;
        countInLeft = 0;
        countInPos = 0;

        // count-in: the playhead waits N bars while the click counts
        if (isNowPlaying && countInOnPlay.load() && countInBars.load() > 0)
            countInLeft = (std::int64_t) std::llround (samplesPerBeat * beatsPerBar.load() * countInBars.load());
    }

    if (previewPending.exchange (false))
        metronome.trigger (true);

    bool notesOff = notesOffPending.exchange (false);

    liveBlock.clear();
    liveCollector.removeNextBlockOfMessages (liveBlock, numSamples);

    const auto sr = sampleRate.load();
    const auto startPos = positionSamples.load();
    auto pos = startPos;

    const bool looping = loopOn.load();
    const auto loopStart = (std::int64_t) std::llround (loopStartSeconds.load() * sr);
    const auto loopEnd = (std::int64_t) std::llround (loopEndSeconds.load() * sr);
    int done = 0;

    while (done < numSamples)
    {
        int n = std::min (numSamples - done, scratch.getNumSamples());

        if (isNowPlaying && countInLeft > 0)
        {
            // count-in: clicks only (always audible), song does not move; plug-ins keep running
            n = (int) std::min<std::int64_t> (n, countInLeft);
            float* segL = left + done;
            float* segR = right != nullptr ? right + done : nullptr;
            if (snap != nullptr)
                renderSegment (*snap, segL, segR, pos, n, false, notesOff, done);
            notesOff = false;

            float* chans[] = { segL, segR };
            metronome.process (chans, segR != nullptr ? 2 : 1, n, countInPos, samplesPerBeat, beatsPerBar.load(),
                               clickGain, metroAccent.load());
            countInPos += n;
            countInLeft -= n;
            if (countInLeft == 0)
                metronome.reset();
            done += n;
            continue;
        }

        if (isNowPlaying && looping && pos < loopEnd && pos + n > loopEnd)
            n = (int) (loopEnd - pos);

        float* segL = left + done;
        float* segR = right != nullptr ? right + done : nullptr;

        // plug-ins run even when stopped (reverb tails, releasing notes)
        if (snap != nullptr)
            renderSegment (*snap, segL, segR, pos, n, isNowPlaying, notesOff, done);
        notesOff = false;

        if (isNowPlaying)
        {
            if (metronomeOn.load() && metroOnPlay.load())
            {
                float* chans[] = { segL, segR };
                metronome.process (chans, segR != nullptr ? 2 : 1, n, pos, samplesPerBeat, beatsPerBar.load(),
                                   clickGain, metroAccent.load());
            }

            pos += n;

            if (looping && pos >= loopEnd)
            {
                pos = loopStart;
                metronome.reset();
                notesOff = true;
            }
        }

        else
        {
            float* chans[] = { segL, segR };
            metronome.renderTail (chans, segR != nullptr ? 2 : 1, n, clickGain); // preview click
        }

        done += n;
    }

    countingIn.store (isNowPlaying && countInLeft > 0);

    if (isNowPlaying)
    {
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

namespace
{
    void runProcessor (juce::AudioProcessor& p, float* const* chans, int maxChannels, int n, juce::MidiBuffer& midi) noexcept
    {
        const juce::ScopedLock sl (p.getCallbackLock());

        // the buffer must have as many channels as the plug-in uses (never fewer:
        // the plug-in would write past the end of it)
        const auto needed = std::max (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
        if (p.isSuspended() || needed > maxChannels)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::clear (chans[ch], n);
            return;
        }

        for (int ch = 2; ch < needed; ++ch) // extra channels start silent
            juce::FloatVectorOperations::clear (chans[ch], n);

        juce::AudioBuffer<float> buffer (chans, std::max (2, needed), n); // no allocation: refers to the scratch memory

        p.processBlock (buffer, midi);

        // mono plug-in output: duplicate to the right channel
        if (p.getMainBusNumOutputChannels() == 1)
            juce::FloatVectorOperations::copy (chans[1], chans[0], n);
    }
}

void AudioEngine::renderSegment (const RenderSnapshot& snap, float* outL, float* outR, std::int64_t start, int n,
                                 bool isPlaying, bool sendNotesOff, int blockOffset) noexcept
{
    const auto liveId = liveTrack.load();
    const auto end = start + n;
    const auto numTracks = std::min ((int) snap.tracks.size(), maxTracks);

    float* chans[maxChannels];
    for (int ch = 0; ch < maxChannels; ++ch)
        chans[ch] = scratch.getWritePointer (ch);

    for (int ti = 0; ti < numTracks; ++ti)
    {
        const auto& track = snap.tracks[(size_t) ti];

        // gain ramp from the previous segment's gains (slot reset if another track is here now)
        auto& last = lastGains[(size_t) ti];
        auto& notes = activeNotes[(size_t) ti];
        if (lastGainIds[(size_t) ti] != track.id)
        {
            lastGainIds[(size_t) ti] = track.id;
            last = { track.gainL, track.gainR };
            notes.reset();
        }

        const auto targetL = track.gainL, targetR = track.gainR;
        const bool silent = juce::exactlyEqual (targetL, 0.0f) && juce::exactlyEqual (targetR, 0.0f)
                         && juce::exactlyEqual (last[0], 0.0f) && juce::exactlyEqual (last[1], 0.0f);
        const bool hasProcessors = track.hasProcessors();

        if (! hasProcessors && (silent || track.clips.empty() || ! isPlaying))
        {
            last = { targetL, targetR };
            continue;
        }

        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::clear (chans[ch], n);

        // --- audio clips ------------------------------------------------------
        if (isPlaying)
        {
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
                    chans[0][i] += (srcL[idx] + (srcL[next] - srcL[idx]) * frac) * env;
                    chans[1][i] += (srcR[idx] + (srcR[next] - srcR[idx]) * frac) * env;
                }
            }
        }

        // --- instrument ---------------------------------------------------------
        if (track.instrument != nullptr)
        {
            midi.clear();

            if (sendNotesOff)
            {
                for (int note = 0; note < 128; ++note)
                    if (notes.test ((size_t) note))
                        midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                notes.reset();
            }

            if (isPlaying && ! track.midi.empty())
            {
                auto it = std::lower_bound (track.midi.begin(), track.midi.end(), start,
                                            [] (const RenderSnapshot::MidiEvent& e, std::int64_t t) { return e.time < t; });

                for (; it != track.midi.end() && it->time < end; ++it)
                {
                    const juce::uint8 bytes[] = { it->status, it->data1, it->data2 };
                    midi.addEvent (bytes, 3, (int) (it->time - start));

                    if (it->status == 0x90)
                        notes.set (it->data1);
                    else
                        notes.reset (it->data1);
                }
            }

            // live input (keyboard / on-screen keyboard) for the selected instrument track
            if (track.id.value == liveId && ! liveBlock.isEmpty())
            {
                for (const auto meta : liveBlock)
                {
                    const auto pos = meta.samplePosition - blockOffset;
                    if (pos < 0 || pos >= n)
                        continue;
                    midi.addEvent (meta.data, meta.numBytes, pos);
                    const auto msg = meta.getMessage();
                    if (msg.isNoteOn())
                        notes.set ((size_t) msg.getNoteNumber());
                    else if (msg.isNoteOff())
                        notes.reset ((size_t) msg.getNoteNumber());
                }
            }

            runProcessor (*track.instrument, chans, maxChannels, n, midi);
        }

        // --- insert effects -----------------------------------------------------
        for (auto& p : track.inserts)
        {
            midi.clear();
            runProcessor (*p, chans, maxChannels, n, midi);
        }

        // --- volume / pan -------------------------------------------------------
        const auto stepL = (targetL - last[0]) / (float) n;
        const auto stepR = (targetR - last[1]) / (float) n;
        auto gL = last[0], gR = last[1];

        for (int i = 0; i < n; ++i)
        {
            gL += stepL;
            gR += stepR;
            outL[i] += chans[0][i] * gL;
            if (outR != nullptr)
                outR[i] += chans[1][i] * gR;
        }

        last = { targetL, targetR };
    }
}

} // namespace sms
