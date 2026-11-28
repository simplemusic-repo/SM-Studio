// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace sms
{

/** Instruments that ship with SM Studio (no plug-in needed).
    Identified by plug-in ids starting with "builtin:". */
namespace builtin
{
    struct Info
    {
        juce::String id, name;
        bool isInstrument;
    };

    std::vector<Info> list();

    bool isBuiltIn (const juce::String& pluginId);

    /** Creates the processor, or nullptr for unknown ids. */
    std::unique_ptr<juce::AudioProcessor> create (const juce::String& pluginId);
}

} // namespace sms
