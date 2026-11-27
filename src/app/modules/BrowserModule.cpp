// SPDX-License-Identifier: AGPL-3.0-or-later
// Browser tabs (instruments, effects, files) and what happens when their
// items are dropped on the arrangement.
//
// Until plug-in hosting exists (stage 3), the instrument and effect lists
// contain built-in placeholders so drag & drop can already be used and tested.
// The plug-in host module will register real sources with the same API.

#include "Modules.h"

#include "SongHelpers.h"
#include "core/Commands.h"
#include "extension/ExtensionRegistry.h"

namespace sms
{

namespace
{
    const juce::String placeholderCategory = "SM Studio (Platzhalter bis Plug-in-Hosting)";

    class StaticSource final : public BrowserSource
    {
    public:
        StaticSource (juce::String i, juce::String t, std::vector<BrowserItem> it)
            : id (std::move (i)), title (std::move (t)), items (std::move (it)) {}

        juce::String getId() const override { return id; }
        juce::String getTitle() const override { return title; }
        std::vector<BrowserItem> getItems() override { return items; }

    private:
        juce::String id, title;
        std::vector<BrowserItem> items;
    };

    /** Audio files from the user's music folder (scanned on a background thread). */
    class FileSource final : public BrowserSource,
                             private juce::Thread,
                             private juce::AsyncUpdater
    {
    public:
        FileSource() : juce::Thread ("SM Studio file scan") { startThread (juce::Thread::Priority::low); }
        ~FileSource() override { stopThread (2000); }

        juce::String getId() const override { return "files"; }
        juce::String getTitle() const override { return "Dateien"; }

        std::vector<BrowserItem> getItems() override
        {
            const juce::ScopedLock sl (lock);
            return items;
        }

    private:
        void run() override
        {
            std::vector<BrowserItem> found;
            const auto root = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
            constexpr int maxFiles = 400;

            if (root.isDirectory())
            {
                for (const auto& entry : juce::RangedDirectoryIterator (root, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg",
                                                                        juce::File::findFiles))
                {
                    if (threadShouldExit() || (int) found.size() >= maxFiles)
                        break;

                    const auto f = entry.getFile();
                    found.push_back ({ "audiofile", f.getFullPathName(), f.getFileName(),
                                       f.getParentDirectory().getRelativePathFrom (root.getParentDirectory()) });
                }
            }

            {
                const juce::ScopedLock sl (lock);
                items = std::move (found);
            }

            triggerAsyncUpdate(); // notify on the message thread
        }

        void handleAsyncUpdate() override
        {
            if (onItemsChanged)
                onItemsChanged();
        }

        juce::CriticalSection lock;
        std::vector<BrowserItem> items;
    };

    //==========================================================================
    class BrowserModule final : public Module
    {
    public:
        juce::String getId() const override { return "sms.browser"; }
        juce::String getName() const override { return "Browser"; }

        void initialise (AppContext& ctx) override
        {
            auto& reg = ctx.registry;

            reg.addBrowserSource (std::make_shared<StaticSource> ("instruments", "Instrumente", std::vector<BrowserItem> {
                { "instrument", "builtin:testsynth", "SM Testsynth", placeholderCategory },
                { "instrument", "builtin:sampler", "SM Sampler", placeholderCategory },
                { "instrument", "builtin:drums", "SM Drums", placeholderCategory },
            }));

            reg.addBrowserSource (std::make_shared<StaticSource> ("effects", "Effekte", std::vector<BrowserItem> {
                { "effect", "builtin:eq", "SM EQ", placeholderCategory },
                { "effect", "builtin:compressor", "SM Kompressor", placeholderCategory },
                { "effect", "builtin:reverb", "SM Reverb", placeholderCategory },
                { "effect", "builtin:delay", "SM Delay", placeholderCategory },
            }));

            fileSource = std::make_shared<FileSource>();
            reg.addBrowserSource (fileSource);

            // instrument: on an instrument track -> replace its instrument,
            // anywhere else -> new instrument track
            reg.addDropHandler ({ "instrument", [] (AppContext& c, const BrowserItem& item, const DropTarget& target)
            {
                auto* track = c.song.findTrack (target.trackId);

                if (track != nullptr && track->typeId == "instrument")
                {
                    c.selection.selectTrack (track->id);
                    return c.perform (std::make_unique<commands::SetInstrument> (track->id, item.id, item.name));
                }

                auto t = createTrack (c, "instrument", item.name);
                t.instrumentPluginId = item.id;
                t.instrumentName = item.name;
                const auto id = t.id;
                const auto index = track != nullptr ? c.song.indexOfTrack (track->id) + 1 : target.insertIndex;

                if (c.perform (std::make_unique<commands::AddTrack> (std::move (t), index)))
                {
                    c.selection.selectTrack (id);
                    return true;
                }
                return false;
            } });

            // effect: insert at the end of the track's chain
            reg.addDropHandler ({ "effect", [] (AppContext& c, const BrowserItem& item, const DropTarget& target)
            {
                if (c.song.findTrack (target.trackId) == nullptr)
                    return false;

                InsertSlot slot;
                slot.id = c.song.newId();
                slot.pluginId = item.id;
                slot.name = item.name;
                c.selection.selectTrack (target.trackId);
                return c.perform (std::make_unique<commands::AddInsert> (target.trackId, slot));
            } });

            reg.addDropHandler ({ "audiofile", [] (AppContext& c, const BrowserItem& item, const DropTarget& target)
            {
                return importAudioFile (c, juce::File (item.id), target);
            } });
        }

        void shutdown() override { fileSource.reset(); }

    private:
        std::shared_ptr<FileSource> fileSource;
    };
}

std::unique_ptr<Module> createBrowserModule() { return std::make_unique<BrowserModule>(); }

} // namespace sms
