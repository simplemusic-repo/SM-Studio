# Roadmap

Grobe Etappen bis zu einer brauchbaren Version. Eine Etappe ≈ 1–3 Arbeitssitzungen.

## ✅ Etappe 1 – Grundgerüst
- [x] CMake + JUCE 9, Builds für Windows/macOS über GitHub Actions, Unit-Tests
- [x] Datenmodell (Song, Track, Region, AudioSource, IDs, TempoMap), ARA-tauglich
- [x] Command-System mit Undo/Redo, Gesten-Zusammenfassung, Transaktionen
- [x] Modul-/Extension-System (Aktionen, Panels, Spurtypen, Browser-Quellen, Drop-Handler)
- [x] Oberfläche: Toolbar, Inspector, Arrangement mit Lineal, Browser, Konsole, Transportleiste
- [x] Drag & Drop: Instrument → Spur, Effekt → Insert, Audiodatei → Audiospur (+ aus Explorer/Finder)
- [x] Audio-Engine mit Transport und Metronom

## Etappe 2 – Arrangement & Audio-Wiedergabe
- [ ] Render-Snapshots für die Engine, Audio-Regionen abspielen (mit Lautstärke/Pan/Mute/Solo)
- [ ] Wellenform-Darstellung (Peak-Cache)
- [ ] Events auswählen, verschieben, kopieren, Länge ändern, teilen, löschen (Werkzeuge)
- [ ] Spuren per Drag & Drop umsortieren, Spurhöhe ändern
- [ ] Loop-Bereich

## Etappe 3 – Plug-ins
- [ ] VST3-Hosting (Windows/macOS), AU (macOS), Plug-in-Scan in separatem Prozess
- [ ] Browser zeigt echte Instrumente/Effekte nach Hersteller/Kategorie
- [ ] Plug-in-Fenster öffnen, Inserts umsortieren/bypass, Latenzausgleich
- [ ] Instrumentenspuren spielen MIDI → Instrument

## Etappe 4 – Piano Roll
- [ ] Editor unten (F2), Noten zeichnen/verschieben/löschen, Velocity, Quantisieren

## Etappe 5 – Konsole
- [ ] Busse, Sends, Ausgänge, Pegelanzeigen pro Kanal, Inserts in der Konsole

## Etappe 6 – Automation
- [ ] Automationsspuren für Mixer- und Plug-in-Parameter

## Etappe 7 – Aufnahme
- [ ] Audio- und MIDI-Aufnahme, Monitoring, Metronom-Vorzähler, ASIO unter Windows

## Etappe 8 – Projekte & Export
- [ ] Speichern/Laden (Projektdatei + Audio-Pool), Autosave, zuletzt geöffnete Projekte
- [ ] Export als WAV/FLAC/MP3, Einstellungen (Audiogerät, MIDI)

## Etappe 9 – ARA
- [ ] ARA-2-Host (Melodyne & Co.), Editor-Bereich für ARA-Plug-ins

## Danach
- Erweiterungen als DLL/dylib (C-ABI), Mehrsprachigkeit, Tempospur, Arranger-Spur,
  Comping, Ordnerspuren, Video …
