// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ValueFormat.h"

#include "Model.h"

namespace sms::valueformat
{

namespace
{
    /** Extracts a number like "-3,5" -> -3.5. Empty if there are no digits. */
    std::optional<double> parseNumber (const juce::String& text)
    {
        const auto cleaned = text.retainCharacters ("+-0123456789.,").replaceCharacter (',', '.');
        if (! cleaned.containsAnyOf ("0123456789"))
            return std::nullopt;
        return cleaned.getDoubleValue();
    }
}

juce::String formatDb (float db)
{
    if (db <= Track::minVolumeDb + 0.01f)
        return "-inf dB";

    auto rounded = std::round (db * 10.0f) / 10.0f;
    if (juce::exactlyEqual (rounded, -0.0f))
        rounded = 0.0f;

    return (rounded > 0.0f ? "+" : "") + juce::String (rounded, 1) + " dB";
}

std::optional<float> parseDb (const juce::String& text)
{
    const auto t = text.trim().toLowerCase();
    if (t.isEmpty())
        return std::nullopt;

    if (t.contains ("inf"))
        return t.startsWithChar ('+') ? Track::maxVolumeDb : Track::minVolumeDb;

    if (auto n = parseNumber (t))
        return juce::jlimit (Track::minVolumeDb, Track::maxVolumeDb, (float) *n);

    return std::nullopt;
}

juce::String formatPan (float pan)
{
    const auto v = juce::roundToInt (pan * 100.0f);
    if (v == 0)
        return "C";
    return (v < 0 ? "L" : "R") + juce::String (std::abs (v));
}

std::optional<float> parsePan (const juce::String& text)
{
    const auto t = text.trim().toUpperCase();
    if (t.isEmpty())
        return std::nullopt;

    if (t == "C" || t == "M" || t == "MITTE" || t == "CENTER")
        return 0.0f;

    auto n = parseNumber (t);
    if (! n)
        return std::nullopt;

    auto value = std::abs (*n);
    if (t.startsWithChar ('L'))
        value = -value;
    else if (t.startsWithChar ('R'))
        value = +value;
    else
        value = *n; // plain number: negative = left, positive = right

    return juce::jlimit (-1.0f, 1.0f, (float) value / 100.0f);
}

} // namespace sms::valueformat
