// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>

namespace sms::crashinfo
{

/** A few recent "what happened" notes (message thread), written into crash.log
    if SM Studio crashes - e.g. "loading plug-in Kontakt 8". Not real-time safe:
    never call from the audio thread. */
struct Notes
{
    static constexpr int size = 8;
    std::array<juce::String, size> entries;
    std::atomic<int> next { 0 };
};

inline Notes& notes()
{
    static Notes n;
    return n;
}

inline void note (const juce::String& text)
{
    auto& n = notes();
    const auto i = n.next.fetch_add (1);
    n.entries[(size_t) (i % Notes::size)] = juce::Time::getCurrentTime().toString (false, true, true, true) + "  " + text;
}

/** Oldest first. */
inline juce::StringArray recent()
{
    auto& n = notes();
    juce::StringArray out;
    const auto end = n.next.load();
    for (int i = std::max (0, end - Notes::size); i < end; ++i)
        out.add (n.entries[(size_t) (i % Notes::size)]);
    return out;
}

} // namespace sms::crashinfo
