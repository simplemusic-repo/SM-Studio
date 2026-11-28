// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "core/Ids.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>
#include <memory>

namespace sms
{

/** Knows which plug-ins are installed (VST3, and AU on macOS) and creates
    instances of them.

    Scanning runs on a background thread. A "dead man's pedal" file records the
    plug-in being scanned; if it crashes SM Studio, it is skipped (blacklisted)
    on the next scan. The list is stored in the user's settings folder.

    Instances are kept per owner (a track id for instruments, an insert slot id
    for effects) so undo/redo reuses the same instance with its settings.
    Listeners are notified (on the message thread) when the plug-in list or an
    instance changed. */
class PluginHost final : public juce::ChangeBroadcaster,
                         private juce::Thread,
                         private juce::AsyncUpdater
{
public:
    /** @param dataFolder  where the plug-in list is stored (empty: nothing is stored) */
    explicit PluginHost (juce::File dataFolder);
    ~PluginHost() override;

    // --- installed plug-ins ---------------------------------------------------
    void startScan();
    bool isScanning() const { return isThreadRunning(); }
    juce::String getStatusText() const;

    struct Entry
    {
        juce::String id;           // identifier string stored in the song
        juce::String name, manufacturer, format;
        bool isInstrument = false;
    };

    /** Built-in instruments plus all scanned plug-ins. */
    std::vector<Entry> getEntries() const;
    juce::String getName (const juce::String& pluginId) const;

    // --- instances --------------------------------------------------------------
    /** The instance for an owner, or nullptr while it is being created (it is
        then created asynchronously and listeners are notified) or if it failed. */
    std::shared_ptr<juce::AudioProcessor> getInstance (ObjectId owner, const juce::String& pluginId);

    /** Error message if creating the instance failed. */
    juce::String getInstanceError (ObjectId owner) const;

    /** Sample rate / maximum block size used for new instances. */
    void setPlaybackFormat (double sampleRate, int blockSize) { currentRate = sampleRate; currentBlock = blockSize; }

    /** Removes all instances (song closed). */
    void clearInstances();

    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formats; }

private:
    void run() override;
    void handleAsyncUpdate() override { sendChangeMessage(); }
    void saveList();
    void loadList();
public:
    /** Prepares a processor (only while the audio thread is not using it). */
    static void prepare (juce::AudioProcessor&, double sampleRate, int blockSize);

private:
    struct Instance
    {
        juce::String pluginId;
        std::shared_ptr<juce::AudioProcessor> processor;
        bool pending = false;
        juce::String error;
        int generation = 0;
    };

    juce::File dataFolder;
    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList known;
    mutable juce::CriticalSection statusLock;
    juce::String scanningName;
    std::map<ObjectId, Instance> instances;
    double currentRate = 44100.0;
    int currentBlock = 512;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

} // namespace sms
