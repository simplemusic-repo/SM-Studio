// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace sms::valueformat
{

/** "-3.5 dB", "-inf dB" */
juce::String formatDb (float db);

/** Accepts "-3.5", "-3,5 dB", "+2", "-inf", "inf". Result is clamped to the track volume range. */
std::optional<float> parseDb (const juce::String&);

/** "C", "L30", "R100" (pan is -1..+1) */
juce::String formatPan (float pan);

/** Accepts "C", "L30", "R 20", "30" (right), "-30" (left), "l100". Result in -1..+1. */
std::optional<float> parsePan (const juce::String&);

} // namespace sms::valueformat
