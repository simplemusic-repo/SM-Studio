// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Metronome.h"
#include "RenderSnapshot.h"
#include "core/MusicalTime.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>

namespace sms
{

/** Owns the audio device and runs the real-time audio callback.

    Threading rules:
      - All public functions except renderBlock() are called from the message thread.
      - The audio thread never locks or allocates. It reads transport state from
        atomics and the song from an immutable RenderSnapshot.
      - Snapshots are exchanged with a hazard pointer: the audio thread announces
        which snapshot it is using, the message thread only deletes snapshots that
        are neither current nor in use (see collectGarbage()).
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

    /** For offline rendering / tests without a device. */
    void prepareToPlay (double sampleRate, int maxBlockSize);

    //==========================================================================
    // Transport
    void play() noexcept { playing.store (true); }
    void stop() noexcept { playing.store (false); }
    void togglePlay() noexcept { playing.store (! playing.load()); }
    bool isPlaying() const noexcept { return playing.load(); }

    void setPositionSeconds (double seconds) noexcept;
    double getPositionSeconds() const noexcept;
    std::int64_t getPositionSamples() const noexcept { return positionSamples.load(); }

    void setTempo (double bpm, TimeSignature sig) noexcept;

    /** Loop range in seconds. When enabled, playback jumps from end to start. */
    void setLoop (double startSeconds, double endSeconds, bool enabled) noexcept;

    void setMetronomeEnabled (bool b) noexcept { metronomeOn.store (b); }
    bool isMetronomeEnabled() const noexcept { return metronomeOn.load(); }

    void setMasterGainDb (float db) noexcept;

    //==========================================================================
    // Song data
    /** Makes a new snapshot current (message thread). */
    void publish (std::unique_ptr<RenderSnapshot>);

    /** Frees snapshots the audio thread no longer uses (message thread, call regularly). */
    void collectGarbage();

    //==========================================================================
    // Status for the UI
    double getSampleRate() const noexcept { return sampleRate.load(); }
    int getBufferSize() const noexcept { return bufferSize.load(); }
    double getCpuUsage() const { return deviceManager.getCpuUsage(); }
    juce::String getDeviceName() const;

    /** Highest absolute sample value since the last call (channel 0 or 1). */
    float getAndResetPeak (int channel) noexcept;

    /** The real-time render function (called by the device callback, or by tests). */
    void renderBlock (float* const* outputs, int numOutputs, int numSamples) noexcept;

    static constexpr int maxTracks = 1024;

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    void renderSegment (const RenderSnapshot&, float* left, float* right, std::int64_t start, int n) noexcept;

    juce::AudioDeviceManager deviceManager;

    std::atomic<bool> playing { false }, metronomeOn { false }, loopOn { false };
    std::atomic<std::int64_t> positionSamples { 0 };
    std::atomic<double> loopStartSeconds { 0.0 }, loopEndSeconds { 0.0 };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> bufferSize { 0 };
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> beatsPerBar { 4 }, beatUnit { 4 };
    std::atomic<float> masterGain { 1.0f };
    std::atomic<float> peaks[2] {};

    // snapshot exchange
    std::atomic<RenderSnapshot*> current { nullptr };
    std::atomic<RenderSnapshot*> inUse { nullptr };
    std::vector<std::unique_ptr<RenderSnapshot>> owned; // message thread only

    // audio thread state (preallocated)
    Metronome metronome;
    bool wasPlaying = false;
    juce::AudioBuffer<float> scratch;
    std::vector<std::array<float, 2>> lastGains;
    std::vector<ObjectId> lastGainIds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace sms
