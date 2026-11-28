// SPDX-License-Identifier: AGPL-3.0-or-later
#include "engine/Metronome.h"
#include "engine/PluginHost.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sms
{

class MetronomeTests final : public juce::UnitTest
{
public:
    MetronomeTests() : juce::UnitTest ("Metronome", "SMStudio") {}

    void runTest() override
    {
        beginTest ("clicks land exactly on the beats, across block boundaries");

        constexpr double sr = 48000.0;
        constexpr double bpm = 120.0;
        constexpr double samplesPerBeat = sr * 60.0 / bpm; // 24000
        constexpr int total = (int) (samplesPerBeat * 4) + 100;

        for (int blockSize : { 64, 441, 512, 4096 })
        {
            Metronome m;
            m.prepare (sr);
            juce::AudioBuffer<float> buffer (2, total);
            buffer.clear();

            for (int start = 0; start < total; start += blockSize)
            {
                const auto n = std::min (blockSize, total - start);
                float* chans[] = { buffer.getWritePointer (0, start), buffer.getWritePointer (1, start) };
                m.process (chans, 2, n, start, samplesPerBeat, 4, 1.0f);
            }

            // each beat starts with silence-to-signal; check onsets
            for (int beat = 0; beat < 4; ++beat)
            {
                const auto onset = (int) (beat * samplesPerBeat);
                expect (buffer.getMagnitude (0, onset, 200) > 0.05f, "click missing at beat " + juce::String (beat));
                if (onset > 2000)
                    expect (buffer.getMagnitude (0, onset - 1000, 900) < 1.0e-6f, "signal before beat " + juce::String (beat));
            }

            // accent on beat 1 is louder than beat 2
            expect (buffer.getMagnitude (0, 0, 2000) > buffer.getMagnitude (0, (int) samplesPerBeat, 2000));
        }
    }
};

static MetronomeTests metronomeTests;

//==============================================================================
class PluginHostInfoTests final : public juce::UnitTest
{
public:
    PluginHostInfoTests() : juce::UnitTest ("PluginHost info", "SMStudio") {}

    void runTest() override
    {
        beginTest ("plug-in categories become short German type names");
        expectEquals (PluginHost::describeCategory ("Fx|Reverb", false), juce::String ("Hall"));
        expectEquals (PluginHost::describeCategory ("Fx|Dynamics", false), juce::String ("Dynamik"));
        expectEquals (PluginHost::describeCategory ("Instrument|Synth", true), juce::String ("Synthesizer"));
        expectEquals (PluginHost::describeCategory ("Fx|Stereo|EQ", false), juce::String ("EQ"));
        expectEquals (PluginHost::describeCategory ("", false), juce::String ("Sonstige"));
        expectEquals (PluginHost::describeCategory ("Instrument", true), juce::String ("Instrument"));
        expectEquals (PluginHost::describeCategory ("Fx|Gizmo", false), juce::String ("Gizmo"));

        beginTest ("usage counts are stored and can be removed");
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sms-usage-test");
        folder.deleteRecursively();
        {
            PluginHost host (folder);
            host.noteUsed ("a");
            host.noteUsed ("a");
            host.noteUsed ("b");
            expectEquals (host.getUsageCount ("a"), 2);
            expectEquals (host.getUsageCount ("c"), 0);
        }
        {
            PluginHost host (folder); // reloaded from usage.xml
            expectEquals (host.getUsageCount ("a"), 2);
            expectEquals (host.getUsageCount ("b"), 1);
            host.forgetUsage ("a");
            expectEquals (host.getUsageCount ("a"), 0);
        }
        {
            PluginHost host (folder);
            expectEquals (host.getUsageCount ("a"), 0);
            expectEquals (host.getUsageCount ("b"), 1);
        }
        folder.deleteRecursively();
    }
};

static PluginHostInfoTests pluginHostInfoTests;

} // namespace sms
