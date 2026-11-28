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

    /** Audio files of a folder (default: the user's music folder), scanned on a
        background thread. The folder can be changed and is stored in the settings. */
    class FileSource final : public BrowserSource,
                             private juce::Thread,
                             private juce::AsyncUpdater
    {
    public:
        explicit FileSource (juce::PropertySet& s) : juce::Thread ("SM Studio file scan"), settings (s)
        {
            rescan();
        }

        ~FileSource() override
        {
            stopThread (4000);
            cancelPendingUpdate();
        }

        juce::String getId() const override { return "files"; }
        juce::String getTitle() const override { return "Dateien"; }

        std::vector<BrowserItem> getItems() override
        {
            const juce::ScopedLock sl (lock);
            return items;
        }

        juce::String getStatusText() override
        {
            const juce::ScopedLock sl (lock);
            const auto root = getRoot();
            if (scanning.load())
                return juce::String (u8"Durchsuche ") + root.getFullPathName() + juce::String (u8" …");
            if (! root.isDirectory())
                return "Ordner nicht gefunden: " + root.getFullPathName();
            if (items.empty())
                return "Keine Audiodateien in " + root.getFullPathName()
                     + juce::String (u8" – mit „Ordner …“ einen anderen Ordner wählen");
            return juce::String ((int) items.size()) + (truncated ? "+ Dateien in " : " Dateien in ") + root.getFullPathName();
        }

        std::vector<Button> getButtons() override
        {
            return {
                { juce::String (u8"Ordner …"), juce::String (u8"Ordner mit Audiodateien wählen"), [this] { chooseFolder(); } },
                { "Aktualisieren", "Ordner neu einlesen", [this] { rescan(); } },
            };
        }

        juce::File getRoot() const
        {
            const auto saved = settings.getValue ("browser.files.folder");
            if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved))
                return juce::File (saved);
            return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        }

        void rescan()
        {
            stopThread (4000);
            scanning = true;
            startThread (juce::Thread::Priority::low);
            if (onItemsChanged)
                onItemsChanged();
        }

    private:
        void chooseFolder()
        {
            chooser = std::make_unique<juce::FileChooser> (u8"Ordner mit Audiodateien wählen", getRoot());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto dir = fc.getResult();
                                      if (dir.isDirectory())
                                      {
                                          settings.setValue ("browser.files.folder", dir.getFullPathName());
                                          rescan();
                                      }
                                  });
        }

        void run() override
        {
            std::vector<BrowserItem> found;
            const auto root = getRoot();
            constexpr int maxFiles = 3000;
            bool hitLimit = false;

            if (root.isDirectory())
            {
                for (const auto& entry : juce::RangedDirectoryIterator (root, true, "*",
                                                                        juce::File::findFiles | juce::File::ignoreHiddenFiles))
                {
                    if (threadShouldExit())
                        return;
                    if ((int) found.size() >= maxFiles)
                    {
                        hitLimit = true;
                        break;
                    }

                    const auto f = entry.getFile();
                    if (! f.hasFileExtension ("wav;wave;aif;aiff;flac;mp3;ogg"))
                        continue;

                    auto folder = f.getParentDirectory().getRelativePathFrom (root);
                    if (folder == ".")
                        folder = root.getFileName();
                    found.push_back ({ "audiofile", f.getFullPathName(), f.getFileName(), folder });
                }
            }

            std::sort (found.begin(), found.end(), [] (auto& a, auto& b)
            {
                const auto c = a.category.compareNatural (b.category);
                return c != 0 ? c < 0 : a.name.compareNatural (b.name) < 0;
            });

            {
                const juce::ScopedLock sl (lock);
                items = std::move (found);
                truncated = hitLimit;
            }
            scanning = false;

            triggerAsyncUpdate(); // notify on the message thread
        }

        void handleAsyncUpdate() override
        {
            if (onItemsChanged)
                onItemsChanged();
        }

        juce::PropertySet& settings;
        juce::CriticalSection lock;
        std::vector<BrowserItem> items;
        bool truncated = false;
        std::atomic<bool> scanning { false };
        std::unique_ptr<juce::FileChooser> chooser;
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

            fileSource = std::make_shared<FileSource> (ctx.settings);
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
