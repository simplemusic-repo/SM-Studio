// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "core/Ids.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <map>
#include <memory>

namespace sms
{

/** Knows which plug-ins are installed (VST3, and AU on macOS) and creates
    instances of them.

    Scanning runs on a background thread. Every plug-in file is checked in a
    separate child process (SM Studio started with --scan-plugin): if it
    crashes or takes longer than 30 seconds, it is killed and the plug-in is
    skipped (blacklisted). The list is stored in the user's settings folder.

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

    /** Asks the scan to stop after the current plug-in (the list found so far is kept). */
    void cancelScan() { signalThreadShouldExit(); }

    /** Forgets plug-ins that were skipped because they crashed during a scan. */
    void clearSkippedPlugins() { known.clearBlacklistedFiles(); saveList(); sendChangeMessage(); }
    int getNumSkippedPlugins() const { return known.getBlacklistedFiles().size(); }
    juce::StringArray getSkippedPlugins() const { return known.getBlacklistedFiles(); }
    bool isCancelling() const { return isThreadRunning() && threadShouldExit(); }

    /** 0..1 while scanning, and the plug-in currently being checked. */
    float getScanProgress() const noexcept { return scanProgress.load(); }
    juce::String getScanningName() const;

    struct Entry
    {
        juce::String id;           // identifier string stored in the song
        juce::String name, manufacturer, format;
        bool isInstrument = false;
        juce::String category;     // German type name, e.g. "Hall", "Synthesizer"
    };

    /** Turns a plug-in's category string ("Fx|Reverb", "Instrument|Synth", "Dynamics" ...)
        into a short German type name for the browser ("Hall", "Synthesizer", "Dynamik" ...). */
    static juce::String describeCategory (const juce::String& rawCategory, bool isInstrument);

    // --- usage counts ("Favoriten") ---------------------------------------------
    /** Counts that a plug-in was added to a track (stored in usage.xml). */
    void noteUsed (const juce::String& pluginId);
    int getUsageCount (const juce::String& pluginId) const;
    /** Removes a plug-in from the favourites (resets its count). */
    void forgetUsage (const juce::String& pluginId);

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
    int getPlaybackBlockSize() const noexcept { return currentBlock; }

    /** Removes all instances (song closed). */
    void clearInstances();

    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formats; }

    /** Used by the plug-in scanner child process (command line --scan-plugin):
        checks one plug-in file and writes the found plug-ins as XML. */
    static bool scanFileAndWriteResult (const juce::String& formatName, const juce::String& file, const juce::File& resultFile);

private:
    void run() override;
    bool scanFileInChildProcess (const juce::File& executable, juce::AudioPluginFormat&, const juce::String& file);
    void handleAsyncUpdate() override { sendChangeMessage(); }
    void saveList();
    void loadList();
    void saveUsage() const;
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
    std::map<juce::String, int> usage;
    mutable juce::CriticalSection statusLock;
    juce::String scanningName;
    std::atomic<float> scanProgress { 0.0f };
    std::map<ObjectId, Instance> instances;
    double currentRate = 44100.0;
    int currentBlock = 512;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

} // namespace sms
