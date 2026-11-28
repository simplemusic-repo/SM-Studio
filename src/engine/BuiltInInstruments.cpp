// SPDX-License-Identifier: AGPL-3.0-or-later
#include "BuiltInInstruments.h"

namespace sms::builtin
{

namespace
{
    //==========================================================================
    /** "SM Testsynth": simple polyphonic synth (saw + sine, low-pass, ADSR).
        Meant for testing MIDI playback without third-party plug-ins. */
    struct TestSound final : public juce::SynthesiserSound
    {
        bool appliesToNote (int) override { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    struct SynthParams
    {
        juce::AudioParameterFloat* volume = nullptr;
        juce::AudioParameterFloat* attack = nullptr;
        juce::AudioParameterFloat* release = nullptr;
        juce::AudioParameterFloat* brightness = nullptr;
    };

    class TestVoice final : public juce::SynthesiserVoice
    {
    public:
        explicit TestVoice (const SynthParams& p) : params (p) {}

        using juce::SynthesiserVoice::renderNextBlock;

        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<TestSound*> (s) != nullptr; }

        void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
        {
            frequency = juce::MidiMessage::getMidiNoteInHertz (note);
            level = velocity * 0.18f * params.volume->get();
            phase = 0.0;
            filterState = 0.0f;
            env.setSampleRate (getSampleRate());
            env.setParameters ({ params.attack->get(), 0.25f, 0.6f, params.release->get() });
            env.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
                env.noteOff();
            else
            {
                env.reset();
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive())
                return;

            const auto inc = frequency / getSampleRate();
            const auto cutoff = (float) juce::jlimit (0.02, 0.95, frequency * (1.0 + 14.0 * params.brightness->get()) / getSampleRate());

            for (int i = 0; i < num; ++i)
            {
                const auto saw = (float) (2.0 * phase - 1.0);
                const auto sine = (float) std::sin (phase * juce::MathConstants<double>::twoPi);
                filterState += cutoff * ((0.6f * saw + 0.4f * sine) - filterState);
                const auto sample = filterState * level * env.getNextSample();

                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.addSample (ch, start + i, sample);

                phase += inc;
                if (phase >= 1.0)
                    phase -= 1.0;
            }

            if (! env.isActive())
                clearCurrentNote();
        }

    private:
        const SynthParams& params;
        double frequency = 440.0, phase = 0.0;
        float level = 0.0f, filterState = 0.0f;
        juce::ADSR env;
    };

    class TestSynth final : public juce::AudioProcessor
    {
    public:
        TestSynth()
            : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
        {
            addParameter (params.volume = new juce::AudioParameterFloat ({ "volume", 1 }, u8"Lautstärke", 0.0f, 1.5f, 1.0f));
            addParameter (params.attack = new juce::AudioParameterFloat ({ "attack", 1 }, "Attack (s)", 0.001f, 2.0f, 0.005f));
            addParameter (params.release = new juce::AudioParameterFloat ({ "release", 1 }, "Release (s)", 0.01f, 3.0f, 0.25f));
            addParameter (params.brightness = new juce::AudioParameterFloat ({ "brightness", 1 }, "Helligkeit", 0.0f, 1.0f, 0.35f));

            for (int i = 0; i < 16; ++i)
                synth.addVoice (new TestVoice (params));
            synth.addSound (new TestSound());
        }

        using juce::AudioProcessor::processBlock;

        const juce::String getName() const override { return "SM Testsynth"; }
        void prepareToPlay (double sr, int) override { synth.setCurrentPlaybackSampleRate (sr); }
        void releaseResources() override {}

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
        {
            buffer.clear();
            synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
        }

        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.3; }
        juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
        bool hasEditor() const override { return true; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock& block) override
        {
            juce::MemoryOutputStream out (block, false);
            for (auto* p : getParameters())
                out.writeFloat (p->getValue());
        }

        void setStateInformation (const void* data, int size) override
        {
            juce::MemoryInputStream in (data, (size_t) size, false);
            for (auto* p : getParameters())
                if (! in.isExhausted())
                    p->setValueNotifyingHost (in.readFloat());
        }

    private:
        SynthParams params;
        juce::Synthesiser synth;
    };
}

std::vector<Info> list()
{
    return { { "builtin:testsynth", "SM Testsynth", true } };
}

bool isBuiltIn (const juce::String& pluginId)
{
    return pluginId.startsWith ("builtin:");
}

std::unique_ptr<juce::AudioProcessor> create (const juce::String& pluginId)
{
    if (pluginId == "builtin:testsynth")
        return std::make_unique<TestSynth>();
    return nullptr;
}

} // namespace sms::builtin
