// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>

namespace sms
{

/** Stable identity of every object in a song (tracks, regions, audio sources,
    notes, insert slots ...).

    IDs never change during the lifetime of an object and are never reused
    within a song. They survive undo/redo and are written to the project file.
    This matters for ARA: plug-ins identify audio sources and playback regions
    by persistent IDs supplied by the host.
*/
struct ObjectId
{
    std::uint64_t value = 0;

    constexpr bool isValid() const noexcept { return value != 0; }
    constexpr auto operator<=> (const ObjectId&) const = default;
};

/** Hands out unique IDs for one song. */
class IdGenerator
{
public:
    ObjectId next() noexcept { return { ++last }; }

    /** Call after loading a project so new IDs don't collide with loaded ones. */
    void reserveUpTo (std::uint64_t used) noexcept
    {
        if (used > last)
            last = used;
    }

    std::uint64_t getLastIssued() const noexcept { return last; }

private:
    std::uint64_t last = 0;
};

} // namespace sms

template <>
struct std::hash<sms::ObjectId>
{
    std::size_t operator() (const sms::ObjectId& id) const noexcept
    {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
