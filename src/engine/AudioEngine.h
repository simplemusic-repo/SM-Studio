// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Metronome.h"
#include "core/MusicalTime.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>

namespace sms
{

/** Owns the audio device and runs the real-time audio callback.

    Threading rules:
      - All public functions are called from the message thread.
      - Communication with the audio thread uses atomics only (no locks, no
        allocation on the audio thread).
      - Later stages hand the engine immutable "render snapshots" of the song
        (tracks, regions, plug-in graph) via an atomic pointer swap.
*/
class AudioEngine final : private juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    /** Opens the default output device. Returns an error message or empty string. */
    juce::String initialiseAudio();
    void shutdownAudio();

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    bool hasDevice() const { return deviceManager.getCurrentAudioDevice() != nullptr; }

    //==========================================================================
    // Transport
    void play() noexcept { playing.store (true); }
    void stop() noexcept { playing.store (false); }
    void togglePlay() noexcept { playing.store (! playing.load()); }
    bool isPlaying() const noexcept { return playing.load(); }

    void setPositionSeconds (double seconds) noexcept;
    double getPositionSeconds() const noexcept;

    void setTempo (double bpm, TimeSignature sig) noexcept;

    void setMetronomeEnabled (bool b) noexcept { metronomeOn.store (b); }
    bool isMetronomeEnabled() const noexcept { return metronomeOn.load(); }

    void setMasterGainDb (float db) noexcept;

    //==========================================================================
    // Status for the UI
    double getSampleRate() const noexcept { return sampleRate.load(); }
    int getBufferSize() const noexcept { return bufferSize.load(); }
    double getCpuUsage() const { return deviceManager.getCpuUsage(); }
    juce::String getDeviceName() const;

    /** Highest absolute sample value since the last call (channel 0 or 1). */
    float getAndResetPeak (int channel) noexcept;

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    juce::AudioDeviceManager deviceManager;

    std::atomic<bool> playing { false }, metronomeOn { false };
    std::atomic<std::int64_t> positionSamples { 0 };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> bufferSize { 0 };
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> beatsPerBar { 4 }, beatUnit { 4 };
    std::atomic<float> masterGain { 1.0f };
    std::atomic<float> peaks[2] {};

    Metronome metronome;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace sms
