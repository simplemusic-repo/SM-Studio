// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginHost.h"

#include "BuiltInInstruments.h"
#include "CrashInfo.h"

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

    if (auto xml = juce::parseXML (dataFolder.getChildFile ("usage.xml")))
        for (auto* e : xml->getChildWithTagNameIterator ("PLUGIN"))
            if (const auto n = e->getIntAttribute ("count"); n > 0)
                usage[e->getStringAttribute ("id")] = n;
}

void PluginHost::saveUsage() const
{
    if (dataFolder == juce::File())
        return;

    juce::XmlElement xml ("USAGE");
    for (auto& [id, n] : usage)
    {
        auto* e = xml.createNewChildElement ("PLUGIN");
        e->setAttribute ("id", id);
        e->setAttribute ("count", n);
    }
    dataFolder.createDirectory();
    xml.writeTo (dataFolder.getChildFile ("usage.xml"));
}

void PluginHost::noteUsed (const juce::String& pluginId)
{
    if (pluginId.isEmpty())
        return;
    ++usage[pluginId];
    saveUsage();
    sendChangeMessage();
}

int PluginHost::getUsageCount (const juce::String& pluginId) const
{
    const auto it = usage.find (pluginId);
    return it != usage.end() ? it->second : 0;
}

void PluginHost::forgetUsage (const juce::String& pluginId)
{
    if (usage.erase (pluginId) > 0)
    {
        saveUsage();
        sendChangeMessage();
    }
}

juce::String PluginHost::describeCategory (const juce::String& rawCategory, bool isInstrument)
{
    static const std::pair<const char*, const char*> names[] = {
        { "reverb", "Hall" },           { "delay", "Delay" },           { "eq", "EQ" },
        { "dynamics", "Dynamik" },      { "compressor", "Dynamik" },    { "limiter", "Dynamik" },
        { "distortion", "Verzerrung" }, { "modulation", "Modulation" }, { "filter", "Filter" },
        { "pitch", "Tonh\xc3\xb6he" },         { "analyzer", "Analyse" },      { "spatial", "Raum" },
        { "surround", "Raum" },         { "restoration", "Restauration" }, { "mastering", "Mastering" },
        { "tools", "Werkzeuge" },       { "generator", "Generator" },   { "network", "Netzwerk" },
        { "synth", "Synthesizer" },     { "sampler", "Sampler" },       { "drum", "Drums" },
        { "piano", "Piano" },           { "external", "Extern" },
    };

    for (auto part : juce::StringArray::fromTokens (rawCategory, "|", {}))
    {
        part = part.trim().toLowerCase();
        if (part.isEmpty() || part == "fx" || part == "instrument" || part == "stereo" || part == "mono"
            || part == "effect" || part == "up-downmix" || part == "ambisonics")
            continue;

        for (auto& [key, name] : names)
            if (part.contains (key))
                return juce::String (juce::CharPointer_UTF8 (name));

        return part.substring (0, 1).toUpperCase() + part.substring (1);
    }

    return isInstrument ? juce::String ("Instrument") : juce::String ("Sonstige");
}

void PluginHost::saveList()
{
    if (dataFolder == juce::File())
        return;

    dataFolder.createDirectory();
    if (auto xml = known.createXml())
        xml->writeTo (dataFolder.getChildFile ("plugins.xml"));
}

void PluginHost::startScan (bool full)
{
    if (! isThreadRunning())
    {
        fullScan = full;
        startThread (juce::Thread::Priority::low);
    }
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

namespace
{
    constexpr int scanTimeoutMs = 30000; // a plug-in that takes longer is skipped
}

bool PluginHost::scanFileInChildProcess (const juce::File& executable, juce::AudioPluginFormat& format,
                                         const juce::String& file)
{
    const auto result = juce::File::createTempFile (".xml");

    juce::ChildProcess child;
    const juce::StringArray command { executable.getFullPathName(), "--scan-plugin", format.getName(), file,
                                      result.getFullPathName() };

    if (! child.start (command, 0))
    {
        result.deleteFile();
        return false;
    }

    const auto start = juce::Time::getMillisecondCounter();
    bool timedOut = false;

    while (child.isRunning())
    {
        if (threadShouldExit())
        {
            child.kill(); // "Abbrechen": stop immediately, don't judge this plug-in
            result.deleteFile();
            return true;
        }

        if (juce::Time::getMillisecondCounter() - start > (juce::uint32) scanTimeoutMs)
        {
            timedOut = true;
            child.kill();
            break;
        }

        wait (40);
    }

    // a result written before the process hung on exit still counts
    int found = 0;
    if (auto xml = juce::parseXML (result))
    {
        for (auto* e : xml->getChildIterator())
        {
            juce::PluginDescription d;
            if (d.loadFromXml (*e))
            {
                known.addType (d);
                ++found;
            }
        }
    }
    result.deleteFile();

    if (found == 0)
    {
        known.addToBlacklist (file); // crashed, hung (timedOut) or contains nothing usable
        juce::ignoreUnused (timedOut);
    }

    return true;
}

void PluginHost::run()
{
    scanProgress = 0.0f;
    const auto testDelayMs = juce::SystemStats::getEnvironmentVariable ("SMS_TEST_SCAN_DELAY_MS", "0").getIntValue();
    const auto executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    const bool full = fullScan.load();
    if (full)
        known.clearBlacklistedFiles();

    // 1) collect plug-in files of all formats that are new or changed (full: all)
    struct Job { juce::AudioPluginFormat* format; juce::String file; };
    std::vector<Job> jobs;
    juce::StringArray installed;

    for (auto* format : formats.getFormats())
    {
        if (threadShouldExit())
            break;

        {
            const juce::ScopedLock sl (statusLock);
            scanningName = juce::String (u8"Suche Dateien (") + format->getName() + ") …";
        }
        triggerAsyncUpdate();

        for (auto& file : format->searchPathsForPlugins (format->getDefaultLocationsToSearch(), true, false))
        {
            installed.add (file);
            if (full || (! known.getBlacklistedFiles().contains (file) && ! known.isListingUpToDate (file, *format)))
                jobs.push_back ({ format, file });
        }
    }

    // plug-ins that are no longer installed disappear from the list
    if (! threadShouldExit())
        for (auto& t : known.getTypes())
            if (! installed.contains (t.fileOrIdentifier))
                known.removeType (t);

    // 2) check each one in its own process (a crash or hang cannot take SM Studio down)
    for (size_t i = 0; i < jobs.size() && ! threadShouldExit(); ++i)
    {
        {
            const juce::ScopedLock sl (statusLock);
            scanningName = jobs[i].file;
        }
        scanProgress = (float) i / (float) jobs.size();
        triggerAsyncUpdate();

        if (full) // forget what was known about this file: check it from scratch
            for (auto& t : known.getTypes())
                if (t.fileOrIdentifier == jobs[i].file)
                    known.removeType (t);

        if (! scanFileInChildProcess (executable, *jobs[i].format, jobs[i].file))
        {
            // could not start a child process: fall back to scanning here
            juce::OwnedArray<juce::PluginDescription> found;
            jobs[i].format->findAllTypesForFile (found, jobs[i].file);
            for (auto* d : found)
                known.addType (*d);
        }

        if (testDelayMs > 0) // only for UI tests (SMS_TEST_SCAN_DELAY_MS)
            wait (testDelayMs);
    }

    scanProgress = 1.0f;

    {
        const juce::ScopedLock sl (statusLock);
        scanningName = {};
    }

    saveList();
    triggerAsyncUpdate();
}

bool PluginHost::scanFileAndWriteResult (const juce::String& formatName, const juce::String& file, const juce::File& resultFile)
{
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);

    for (auto* format : fm.getFormats())
    {
        if (format->getName() != formatName)
            continue;

        juce::OwnedArray<juce::PluginDescription> found;
        format->findAllTypesForFile (found, file);

        juce::XmlElement root ("PLUGINS");
        for (auto* d : found)
            root.addChildElement (d->createXml().release());

        return root.writeTo (resultFile);
    }
    return false;
}

std::vector<PluginHost::Entry> PluginHost::getEntries() const
{
    std::vector<Entry> out;

    for (auto& b : builtin::list())
        out.push_back ({ b.id, b.name, "SM Studio", "Eingebaut", b.isInstrument,
                         describeCategory (b.isInstrument ? "Instrument|Synth" : "Fx", b.isInstrument) });

    for (auto& d : known.getTypes())
        out.push_back ({ d.createIdentifierString(), d.name, d.manufacturerName.isNotEmpty() ? d.manufacturerName : juce::String ("Unbekannt"),
                         d.pluginFormatName, d.isInstrument, describeCategory (d.category, d.isInstrument) });

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
void PluginHost::configureBuses (juce::AudioProcessor& p)
{
    // Only the main bus is used: stereo in (effects) / stereo out. Extra outputs
    // (e.g. Kontakt's 16 stereo outs) and side-chain inputs stay disabled - they
    // would cost CPU and need more channels than a track has.
    using CS = juce::AudioChannelSet;
    const auto current = p.getBusesLayout();

    auto make = [&] (CS mainIn, CS mainOut)
    {
        auto l = current;
        for (int i = 0; i < l.inputBuses.size(); ++i)
            l.inputBuses.getReference (i) = i == 0 ? mainIn : CS::disabled();
        for (int i = 0; i < l.outputBuses.size(); ++i)
            l.outputBuses.getReference (i) = i == 0 ? mainOut : CS::disabled();
        return l;
    };

    const juce::AudioProcessor::BusesLayout candidates[] = {
        make (CS::stereo(), CS::stereo()),
        make (CS::disabled(), CS::stereo()),
        make (CS::mono(), CS::stereo()),
        make (CS::mono(), CS::mono()),
        make (CS::disabled(), CS::mono()),
        make (current.getMainInputChannelSet(), current.getMainOutputChannelSet()),
    };

    for (auto& l : candidates)
        if (p.checkBusesLayoutSupported (l) && p.setBusesLayout (l))
            return;
}

void PluginHost::prepare (juce::AudioProcessor& p, double sampleRate, int blockSize)
{
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
    crashinfo::note ("loading plug-in " + desc->name + " (" + desc->pluginFormatName + ", " + desc->fileOrIdentifier + ")");
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
                crashinfo::note ("plug-in created: " + created->getName() + ", configuring buses");
                configureBuses (*created);
                crashinfo::note ("preparing " + created->getName() + " at " + juce::String (currentRate) + " Hz / " + juce::String (currentBlock));
                prepare (*created, currentRate, currentBlock);
                crashinfo::note (created->getName() + " ready (in " + juce::String (created->getTotalNumInputChannels())
                                 + " / out " + juce::String (created->getTotalNumOutputChannels()) + " channels)");
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
