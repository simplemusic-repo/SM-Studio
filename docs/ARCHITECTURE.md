# Architektur

SM Studio ist in Schichten aufgebaut. Jede Schicht kennt nur die darunterliegenden.

```
┌──────────────────────────────────────────────────────────────┐
│ src/app       Anwendung: Fenster, UI, Module, Extension-API   │
│   ├─ extension/   ExtensionApi.h, ExtensionRegistry           │
│   ├─ modules/     eingebaute Module (Core, Transport, …)      │
│   └─ ui/          Theme, ArrangeView, Panels, Bars, Main…     │
├──────────────────────────────────────────────────────────────┤
│ src/engine    Audio: Gerät, Transport, Metronom, (Render-Graph)│
├──────────────────────────────────────────────────────────────┤
│ src/core      Dokument: Song, Track, Region, Commands, Undo    │
├──────────────────────────────────────────────────────────────┤
│ sms_juce      JUCE-Module (einmal kompiliert, statische Lib)   │
└──────────────────────────────────────────────────────────────┘
```

## core – das Dokument

- **`Song`** besitzt Spuren (`Track`), Audioquellen (`AudioSource`) und die `TempoMap`.
- **`Track`** hat einen Typ (`typeId`, z. B. `"audio"`, `"instrument"`), Mixer-Werte,
  eine Insert-Kette (`InsertSlot`) und Events (`Region`).
- **`Region`** ist ein Event auf der Zeitleiste: Audio (nicht-destruktives Fenster auf eine
  `AudioSource`) oder MIDI (Noten).
- Positionen sind **Ticks** (960 pro Viertelnote). Umrechnung in Sekunden/Takte nur über die `TempoMap`.
- Jedes Objekt hat eine **`ObjectId`**, die sich nie ändert und nie wiederverwendet wird.

**ARA-Vorbereitung:** `AudioSource` entspricht einer ARA *audio source*, `Region` einer ARA
*playback region*. Beide haben stabile IDs, Audio wird nie destruktiv verändert. Damit kann
die spätere ARA-Anbindung das Modell direkt abbilden.

### Änderungen nur über Commands

UI-Code ändert den Song **nie direkt**, sondern führt einen `Command` über die
`CommandHistory` aus (`ctx.perform (std::make_unique<commands::...> (...))`). Dadurch ist
alles automatisch rückgängig machbar.

- Kontinuierliche Gesten (Fader ziehen) → `perform (cmd, true)` + `history.endGesture()`
  beim Loslassen: wird zu **einem** Undo-Schritt zusammengefasst (`Command::mergeWith`).
- Mehrere Änderungen als ein Schritt → `beginTransaction ("Name")` … `endTransaction()`.
- Neue Funktion braucht neue Änderung? → neue `Command`-Klasse schreiben (perform/undo).

### Benachrichtigung

`Song::Listener` meldet jede Änderung (`SongChange`: was + welche Spur). Panels hören zu und
aktualisieren sich selbst. `Selection` (nicht rückgängig machbar) meldet Auswahländerungen.

## engine – Audio

- `AudioEngine` öffnet das Audiogerät (JUCE `AudioDeviceManager`) und läuft im Audio-Thread.
- **Regeln für den Audio-Thread:** keine Locks, keine Speicherallokation, kein Zugriff auf `Song`.
  Kommunikation nur über Atomics.
- Ab Etappe 2 bekommt die Engine **Render-Snapshots**: unveränderliche Kopien der
  abspielrelevanten Song-Daten, die per Atomic-Pointer getauscht werden.

## app – Module und Extension-API

Alles, was der Nutzer sieht, wird von **Modulen** registriert (`extension/ExtensionApi.h`):

| Registrierung | Wofür |
|---|---|
| `ActionInfo` | Menüeintrag + Tastaturkürzel + Funktion (Menüs werden daraus gebaut) |
| `PanelInfo` | angedocktes Panel links/rechts/unten (Inspector, Browser, Konsole) |
| `TrackTypeInfo` | Spurtyp (Audio, Instrument, später Bus, Ordner, …) |
| `BrowserSource` | Tab im Browser mit ziehbaren Einträgen |
| `DropHandlerInfo` | was passiert, wenn ein Browser-Eintrag einer Art abgelegt wird |

Module bekommen einen `AppContext` (Song, History, Selection, Engine, Registry, EditState).

### Eine neue Funktion hinzufügen

1. Neue Datei `src/app/modules/MeinModul.cpp` mit einer Klasse `: public Module`.
2. In `initialise (AppContext&)` Aktionen, Panels, Browser-Quellen usw. registrieren.
3. Fabrikfunktion in `Modules.h` deklarieren und in `createBuiltInModules()` eintragen.
4. Datei in `src/app/CMakeLists.txt` aufnehmen.

Der Rest der Anwendung muss dafür nicht angefasst werden.

### Später: Erweiterungen als DLL/dylib

Die Registry-Schnittstellen sind so geschnitten, dass sie später 1:1 als **C-ABI** (wie
VST3/CLAP) angeboten werden können. Ein Loader-Modul lädt dann Dateien aus einem
`Extensions`-Ordner und leitet deren Registrierungen an die Registry weiter. Offene Punkte
dafür: Versionierung der Schnittstelle, Absturz-Schutz (abgesicherter Modus), Lizenz-Ausnahme
für Erweiterungen.

## Threading-Überblick

| Thread | Darf |
|---|---|
| Message-Thread | Song lesen/ändern, UI, Commands |
| Audio-Thread | nur Engine-Daten (Atomics, Snapshots) |
| Hintergrund | z. B. Datei-Scan im Browser; Ergebnisse per `AsyncUpdater` zurück |

## Konventionen

- C++20, JUCE-Stil (4 Leerzeichen, Leerzeichen vor `(` bei Aufrufen).
- Code und Kommentare auf Englisch, Oberfläche auf Deutsch.
- **Nicht-ASCII-Text (Umlaute) immer als `u8"…"`-Literal**, sonst zeigt JUCE ihn falsch an.
  Beim Verketten: `juce::String (u8"…") + x`.
- Jede Datei beginnt mit `// SPDX-License-Identifier: AGPL-3.0-or-later`.
