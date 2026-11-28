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
#include "ui/Theme.h"

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

    //==========================================================================
    // Folder tree ("Dateien"): navigate the file system like in the Explorer /
    // Finder. Shows folders and audio files; files can be dragged into the
    // arrangement or double-clicked. The open folders, the selection and the
    // scroll position are stored and restored on the next start.

    bool isAudioFile (const juce::File& f)
    {
        return f.hasFileExtension ("wav;wave;aif;aiff;flac;mp3;ogg");
    }

    class FileTreeItem final : public juce::TreeViewItem
    {
    public:
        FileTreeItem (AppContext& c, juce::File f, juce::String label = {})
            : ctx (c), file (std::move (f)), isFolder (file.isDirectory()),
              displayName (label.isNotEmpty() ? label : file.getFileName())
        {
            if (displayName.isEmpty())
                displayName = file.getFullPathName(); // drive roots ("C:\")
        }

        bool mightContainSubItems() override { return isFolder; }
        juce::String getUniqueName() const override { return displayName + "|" + file.getFullPathName(); }
        int getItemHeight() const override { return 22; }

        void itemOpennessChanged (bool isNowOpen) override
        {
            if (isNowOpen && ! populated)
                populate();
        }

        void populate()
        {
            populated = true;
            clearSubItems();

            auto children = file.findChildFiles (juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles, false);
            std::vector<juce::File> folders, files;
            for (auto& f : children)
            {
                if (f.isDirectory())
                    folders.push_back (f);
                else if (isAudioFile (f))
                    files.push_back (f);
            }

            auto byName = [] (const juce::File& a, const juce::File& b)
            {
                return a.getFileName().compareNatural (b.getFileName()) < 0;
            };
            std::sort (folders.begin(), folders.end(), byName);
            std::sort (files.begin(), files.end(), byName);

            for (auto& f : folders)
                addSubItem (new FileTreeItem (ctx, f));
            for (auto& f : files)
                addSubItem (new FileTreeItem (ctx, f));
        }

        void refresh()
        {
            if (isFolder && populated)
            {
                const auto open = isOpen();
                populate();
                setOpen (open);
            }
        }

        void paintItem (juce::Graphics& g, int w, int h) override
        {
            if (isSelected())
            {
                g.setColour (theme::colours::accentDim);
                g.fillRect (0, 0, w, h);
            }

            auto iconArea = juce::Rectangle<float> (2.0f, 3.0f, 16.0f, (float) h - 6.0f);
            if (isFolder)
            {
                juce::Path p;
                p.addRoundedRectangle (iconArea.getX(), iconArea.getY() + 3.0f, iconArea.getWidth(), iconArea.getHeight() - 3.0f, 1.5f);
                p.addRoundedRectangle (iconArea.getX(), iconArea.getY() + 1.0f, iconArea.getWidth() * 0.45f, 4.0f, 1.0f);
                g.setColour (juce::Colour (0xffd9b45a));
                g.fillPath (p);
            }
            else
            {
                auto icon = theme::icons::waveform();
                g.setColour (theme::colours::textDim);
                g.fillPath (icon, icon.getTransformToScaleToFit (iconArea, true));
            }

            g.setColour (theme::colours::text);
            g.setFont (theme::font (13.0f));
            g.drawText (displayName, 24, 0, w - 26, h, juce::Justification::centredLeft, true);
        }

        juce::var getDragSourceDescription() override
        {
            if (isFolder)
                return {};
            return BrowserItem { "audiofile", file.getFullPathName(), file.getFileNameWithoutExtension(), {} }.toDragDescription();
        }

        juce::String getTooltip() override { return file.getFullPathName(); }

        void itemDoubleClicked (const juce::MouseEvent&) override
        {
            if (isFolder)
                setOpen (! isOpen());
            else
                activateBrowserItem (ctx, { "audiofile", file.getFullPathName(), file.getFileNameWithoutExtension(), {} });
        }

        void itemClicked (const juce::MouseEvent& e) override
        {
            // single click on a folder opens / closes it, like in most DAW browsers
            if (isFolder && ! e.mods.isPopupMenu() && e.x > 20)
                setOpen (! isOpen());
        }

    private:
        AppContext& ctx;
        juce::File file;
        bool isFolder;
        juce::String displayName;
        bool populated = false;
    };

    /** Invisible root: shortcuts (Musik, Desktop, ...) and the drives. */
    class FileTreeRoot final : public juce::TreeViewItem
    {
    public:
        explicit FileTreeRoot (AppContext& ctx)
        {
            using L = juce::File::SpecialLocationType;
            const std::pair<L, const char*> shortcuts[] = {
                { L::userMusicDirectory, "Musik" },
                { L::userDesktopDirectory, "Desktop" },
                { L::userDocumentsDirectory, "Dokumente" },
                { L::userHomeDirectory, "Benutzerordner" },
            };

            for (auto& [loc, name] : shortcuts)
            {
                auto f = juce::File::getSpecialLocation (loc);
                if (f.isDirectory())
                    addSubItem (new FileTreeItem (ctx, f, juce::String (name)));
            }

            juce::Array<juce::File> roots;
            juce::File::findFileSystemRoots (roots);
           #if JUCE_MAC
            for (auto& v : juce::File ("/Volumes").findChildFiles (juce::File::findDirectories, false))
                roots.addIfNotAlreadyThere (v);
           #endif
            for (auto& r : roots)
                if (r.isDirectory())
                    addSubItem (new FileTreeItem (ctx, r));
        }

        bool mightContainSubItems() override { return true; }
        juce::String getUniqueName() const override { return "root"; }
    };

    class FileTreeView final : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit FileTreeView (AppContext& c) : ctx (c), root (c)
        {
            tree.setRootItem (&root);
            tree.setRootItemVisible (false);
            tree.setDefaultOpenness (false);
            tree.setIndentSize (14);
            tree.setColour (juce::TreeView::backgroundColourId, theme::colours::panelDark);
            tree.setColour (juce::TreeView::linesColourId, theme::colours::separator);
            tree.setColour (juce::TreeView::dragAndDropIndicatorColourId, theme::colours::accent);
            tree.setColour (juce::TreeView::selectedItemBackgroundColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (tree);

            refreshButton.setTooltip (u8"Ordnerinhalte neu einlesen");
            refreshButton.setWantsKeyboardFocus (false);
            refreshButton.onClick = [this] { refreshAll (&root); };
            addAndMakeVisible (refreshButton);

            hint.setText (u8"Dateien ins Arrangement ziehen oder doppelklicken", juce::dontSendNotification);
            hint.setFont (theme::font (12.0f));
            hint.setColour (juce::Label::textColourId, theme::colours::textDim);
            hint.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (hint);

            // restore open folders, selection and scroll position from the last session
            if (auto xml = juce::parseXML (ctx.settings.getValue ("browser.files.tree")))
                tree.restoreOpennessState (*xml, true);
            else
                openMusicFolder();

            savedState = currentState();
            startTimer (1000);
        }

        ~FileTreeView() override
        {
            save();
            tree.setRootItem (nullptr);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto bottom = r.removeFromBottom (24);
            refreshButton.setBounds (bottom.removeFromRight (100).reduced (0, 1));
            hint.setBounds (bottom);
            r.removeFromBottom (4);
            tree.setBounds (r);
        }

    private:
        void openMusicFolder()
        {
            if (root.getNumSubItems() > 0)
            {
                root.getSubItem (0)->setOpen (true);
                tree.scrollToKeepItemVisible (root.getSubItem (0));
            }
        }

        static void refreshAll (juce::TreeViewItem* item)
        {
            for (int i = 0; i < item->getNumSubItems(); ++i)
                if (auto* f = dynamic_cast<FileTreeItem*> (item->getSubItem (i)))
                    if (f->isOpen())
                    {
                        f->refresh();
                        refreshAll (f);
                    }
        }

        juce::String currentState()
        {
            if (auto xml = tree.getOpennessState (true))
                return xml->toString (juce::XmlElement::TextFormat().singleLine());
            return {};
        }

        void save()
        {
            const auto state = currentState();
            if (state != savedState)
            {
                ctx.settings.setValue ("browser.files.tree", state);
                savedState = state;
            }
        }

        void timerCallback() override { save(); } // keep the stored state current (also on crash)

        AppContext& ctx;
        FileTreeRoot root;
        juce::TreeView tree;
        juce::TextButton refreshButton { "Aktualisieren" };
        juce::Label hint;
        juce::String savedState;
    };

    /** The "Dateien" tab: a folder tree view. */
    class FileSource final : public BrowserSource
    {
    public:
        juce::String getId() const override { return "files"; }
        juce::String getTitle() const override { return "Dateien"; }
        std::vector<BrowserItem> getItems() override { return {}; }
        std::unique_ptr<juce::Component> createView (AppContext& ctx) override { return std::make_unique<FileTreeView> (ctx); }
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
