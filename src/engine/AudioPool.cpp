// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioPool.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace sms
{

std::pair<float, float> PeakData::getRange (int channel, std::int64_t start, std::int64_t end) const
{
    if (channel < 0 || channel >= numChannels || end <= start)
        return { 0.0f, 0.0f };

    const auto& ch = peaks[(size_t) channel];
    auto first = (size_t) std::max<std::int64_t> (0, start / samplesPerPeak);
    auto last = (size_t) std::max<std::int64_t> (0, (end - 1) / samplesPerPeak);
    if (first >= ch.size())
        return { 0.0f, 0.0f };
    last = std::min (last, ch.size() - 1);

    float lo = 1.0f, hi = -1.0f;
    for (auto i = first; i <= last; ++i)
    {
        lo = std::min (lo, ch[i].first);
        hi = std::max (hi, ch[i].second);
    }
    return { lo, hi };
}

//==============================================================================
std::shared_ptr<LoadedAudio> AudioPool::loadFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = file.existsAsFile() ? juce::String (u8"Format wird nicht unterstützt") : juce::String ("Datei nicht gefunden");
        return nullptr;
    }

    constexpr std::int64_t maxSamples = (std::int64_t) 48000 * 60 * 30; // 30 minutes at 48 kHz
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > maxSamples)
    {
        error = reader->lengthInSamples <= 0 ? juce::String ("Datei ist leer") : juce::String ("Datei ist zu lang (max. 30 Minuten)");
        return nullptr;
    }

    auto audio = std::make_shared<LoadedAudio>();
    const auto numChannels = (int) juce::jlimit (1u, 2u, reader->numChannels); // stereo for now
    const auto length = (int) reader->lengthInSamples;
    audio->sampleRate = reader->sampleRate;
    audio->buffer.setSize (numChannels, length);
    reader->read (&audio->buffer, 0, length, 0, true, numChannels > 1);

    auto& pd = audio->peaks;
    pd.numChannels = numChannels;
    pd.peaks.resize ((size_t) numChannels);
    const auto blocks = (length + PeakData::samplesPerPeak - 1) / PeakData::samplesPerPeak;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& v = pd.peaks[(size_t) ch];
        v.resize ((size_t) blocks);
        const auto* data = audio->buffer.getReadPointer (ch);

        for (int b = 0; b < blocks; ++b)
        {
            const auto start = b * PeakData::samplesPerPeak;
            const auto n = std::min (PeakData::samplesPerPeak, length - start);
            const auto range = juce::FloatVectorOperations::findMinAndMax (data + start, n);
            v[(size_t) b] = { range.getStart(), range.getEnd() };
        }
    }

    return audio;
}

//==============================================================================
class AudioPool::LoadJob final : public juce::ThreadPoolJob
{
public:
    LoadJob (AudioPool& p, ObjectId sourceId, juce::File f)
        : juce::ThreadPoolJob ("load audio"), pool (p), id (sourceId), file (std::move (f)) {}

    JobStatus runJob() override
    {
        juce::String error;
        auto audio = loadFile (file, error);

        {
            const juce::ScopedLock sl (pool.lock);
            pool.pending.erase (id);
            if (audio != nullptr)
                pool.loaded[id] = std::move (audio);
            else
                pool.errors[id] = error;
        }

        pool.triggerAsyncUpdate();
        return jobHasFinished;
    }

private:
    AudioPool& pool;
    ObjectId id;
    juce::File file;
};

//==============================================================================
AudioPool::AudioPool (Song& s) : song (s)
{
    song.addListener (this);
    syncWithSong();
}

AudioPool::~AudioPool()
{
    song.removeListener (this);
    threads.removeAllJobs (true, 10000);
    cancelPendingUpdate();
}

std::shared_ptr<const LoadedAudio> AudioPool::get (ObjectId id) const
{
    const juce::ScopedLock sl (lock);
    auto it = loaded.find (id);
    return it != loaded.end() ? it->second : nullptr;
}

bool AudioPool::isLoading() const
{
    const juce::ScopedLock sl (lock);
    return ! pending.empty();
}

juce::String AudioPool::getError (ObjectId id) const
{
    const juce::ScopedLock sl (lock);
    auto it = errors.find (id);
    return it != errors.end() ? it->second : juce::String();
}

void AudioPool::waitUntilLoaded (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (isLoading() && juce::Time::getMillisecondCounter() < end)
        juce::Thread::sleep (5);
}

void AudioPool::songChanged (const SongChange& c)
{
    if (c.what == SongChange::What::sourcesChanged || c.what == SongChange::What::reset)
        syncWithSong();
}

void AudioPool::syncWithSong()
{
    const juce::ScopedLock sl (lock);

    // start loading new sources (removed ones stay cached: undo may bring them back)
    for (auto& src : song.getAudioSources())
    {
        if (loaded.count (src.id) || pending.count (src.id) || errors.count (src.id))
            continue;

        pending[src.id] = true;
        threads.addJob (new LoadJob (*this, src.id, src.file), true);
    }
}

void AudioPool::handleAsyncUpdate()
{
    sendChangeMessage();
}

} // namespace sms
