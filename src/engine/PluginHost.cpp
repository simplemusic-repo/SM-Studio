// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginHost.h"

#include "BuiltInInstruments.h"

namespace sms
{

PluginHost::PluginHost (juce::File folder)
    : juce::Thread ("SM Studio plug-in scan"), dataFolder (std::move (folder))
{
    juce::addDefaultFormatsToManager (formats);
    loadList();
}

PluginHost::~PluginHost()
{
    *alive = false;
    stopThread (10000);
    cancelPendingUpdate();
}

//==============================================================================
void PluginHost::loadList()
{
    if (dataFolder == juce::File())
        return;

    if (auto xml = juce::parseXML (dataFolder.getChildFile ("plugins.xml")))
        known.recreateFromXml (*xml);
}

void PluginHost::saveList()
{
    if (dataFolder == juce::File())
        return;

    dataFolder.createDirectory();
    if (auto xml = known.createXml())
        xml->writeTo (dataFolder.getChildFile ("plugins.xml"));
}

void PluginHost::startScan()
{
    if (! isThreadRunning())
        startThread (juce::Thread::Priority::low);
    triggerAsyncUpdate();
}

juce::String PluginHost::getStatusText() const
{
    const auto count = known.getNumTypes();
    const juce::ScopedLock sl (statusLock);

    if (isThreadRunning())
        return juce::String (u8"Suche Plug-ins … ") + scanningName;

    if (count == 0)
        return juce::String (u8"Noch keine Plug-ins gefunden – Einstellungen → Plug-ins suchen.");

    auto text = juce::String (count) + " Plug-ins";
    if (const auto blacklisted = known.getBlacklistedFiles().size(); blacklisted > 0)
        text << " (" << blacklisted << juce::String (u8" übersprungen)");
    return text;
}

juce::String PluginHost::getScanningName() const
{
    const juce::ScopedLock sl (statusLock);
    return scanningName;
}

void PluginHost::run()
{
    scanProgress = 0.0f;
    const auto testDelayMs = juce::SystemStats::getEnvironmentVariable ("SMS_TEST_SCAN_DELAY_MS", "0").getIntValue();
    if (dataFolder != juce::File())
        dataFolder.createDirectory();

    const auto pedal = dataFolder != juce::File() ? dataFolder.getChildFile ("scanning.txt") : juce::File();

    const auto formatList = formats.getFormats();
    const auto numFormats = std::max (1, (int) formatList.size());

    for (int fi = 0; fi < (int) formatList.size(); ++fi)
    {
        if (threadShouldExit())
            break;

        auto* format = formatList[fi];
        juce::PluginDirectoryScanner scanner (known, *format, format->getDefaultLocationsToSearch(), true, pedal, false);
        juce::String name;

        for (;;)
        {
            {
                const juce::ScopedLock sl (statusLock);
                scanningName = scanner.getNextPluginFileThatWillBeScanned();
            }
            scanProgress = ((float) fi + juce::jlimit (0.0f, 1.0f, scanner.getProgress())) / (float) numFormats;
            triggerAsyncUpdate();

            if (threadShouldExit() || ! scanner.scanNextFile (true, name))
                break;

            if (testDelayMs > 0) // only for UI tests (SMS_TEST_SCAN_DELAY_MS)
                wait (testDelayMs);
        }
    }

    scanProgress = 1.0f;

    {
        const juce::ScopedLock sl (statusLock);
        scanningName = {};
    }

    saveList();
    triggerAsyncUpdate();
}

std::vector<PluginHost::Entry> PluginHost::getEntries() const
{
    std::vector<Entry> out;

    for (auto& b : builtin::list())
        out.push_back ({ b.id, b.name, "SM Studio", "Eingebaut", b.isInstrument });

    for (auto& d : known.getTypes())
        out.push_back ({ d.createIdentifierString(), d.name, d.manufacturerName.isNotEmpty() ? d.manufacturerName : juce::String ("Unbekannt"),
                         d.pluginFormatName, d.isInstrument });

    return out;
}

juce::String PluginHost::getName (const juce::String& pluginId) const
{
    for (auto& b : builtin::list())
        if (b.id == pluginId)
            return b.name;

    if (auto d = known.getTypeForIdentifierString (pluginId))
        return d->name;

    return {};
}

//==============================================================================
void PluginHost::prepare (juce::AudioProcessor& p, double sampleRate, int blockSize)
{
    // ask for stereo in/out where the plug-in allows it
    if (auto* inst = dynamic_cast<juce::AudioPluginInstance*> (&p))
        inst->enableAllBuses();

    p.setRateAndBufferSizeDetails (sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
}

std::shared_ptr<juce::AudioProcessor> PluginHost::getInstance (ObjectId owner, const juce::String& pluginId)
{
    if (pluginId.isEmpty())
        return nullptr;

    auto& inst = instances[owner];

    if (inst.pluginId == pluginId && (inst.processor != nullptr || inst.pending || inst.error.isNotEmpty()))
        return inst.processor;

    // new or different plug-in for this owner
    inst.pluginId = pluginId;
    inst.processor = nullptr;
    inst.error = {};
    inst.pending = false;
    const auto generation = ++inst.generation;

    if (builtin::isBuiltIn (pluginId))
    {
        if (auto p = builtin::create (pluginId))
        {
            prepare (*p, currentRate, currentBlock);
            inst.processor = std::shared_ptr<juce::AudioProcessor> (std::move (p));
        }
        else
        {
            inst.error = "Unbekanntes eingebautes Instrument";
        }
        return inst.processor;
    }

    auto desc = known.getTypeForIdentifierString (pluginId);
    if (desc == nullptr)
    {
        inst.error = u8"Plug-in nicht gefunden (nicht installiert oder noch nicht gesucht)";
        return nullptr;
    }

    inst.pending = true;
    formats.createPluginInstanceAsync (*desc, currentRate, currentBlock,
        [this, owner, generation, weak = std::weak_ptr<bool> (alive)] (std::unique_ptr<juce::AudioPluginInstance> created,
                                                                     const juce::String& error)
        {
            if (weak.expired())
                return;

            auto it = instances.find (owner);
            if (it == instances.end() || it->second.generation != generation)
                return; // replaced in the meantime

            auto& entry = it->second;
            entry.pending = false;

            if (created != nullptr)
            {
                prepare (*created, currentRate, currentBlock);
                entry.processor = std::shared_ptr<juce::AudioProcessor> (std::move (created));
            }
            else
            {
                entry.error = error.isNotEmpty() ? error : juce::String ("Plug-in konnte nicht geladen werden");
            }

            sendChangeMessage();
        });

    return nullptr;
}

juce::String PluginHost::getInstanceError (ObjectId owner) const
{
    auto it = instances.find (owner);
    return it != instances.end() ? it->second.error : juce::String();
}

void PluginHost::clearInstances()
{
    instances.clear();
}

} // namespace sms
