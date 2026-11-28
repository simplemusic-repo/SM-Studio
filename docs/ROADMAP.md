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
- [x] Konsole mit großen Fadern, dB-Skala, Zahleneingabe für Lautstärke/Panorama
- [x] Panels abkoppelbar (zweiter Bildschirm), Layout und Fensterpositionen werden gespeichert

## ✅ Etappe 2 – Arrangement & Audio-Wiedergabe
- [x] Render-Snapshots für die Engine, Audio-Events abspielen (mit Lautstärke/Pan/Mute/Solo, Gain-Rampen, Abtastratenwandlung)
- [x] Audio-Pool (Laden im Hintergrund), Wellenform-Darstellung (Peaks)
- [x] Events auswählen, verschieben (auch spurübergreifend), kopieren, Länge ändern, teilen, löschen, stummschalten, duplizieren
- [x] Werkzeuge Pfeil/Teilen/Radiergummi/Stift/Stumm, Auswahlrahmen
- [x] Spuren per Drag & Drop umsortieren
- [x] Loop-Bereich (Lineal, Transport, Tastatur)
- [x] Kontextmenü für Events, Zusammenfügen (G), Ausschneiden/Kopieren/Einfügen
- [x] Raster „Adaptiv“ (abhängig vom Zoom), Panels stufenlos vergrößerbar, Tooltips überall
- [x] Datei-Browser als Ordnerbaum (Zustand wird gespeichert), Inserts in Konsole und Inspector
- [x] Inspector als Kanalzug + Instrument-Bereich (Transponieren/Anschlag gespeichert; wirken ab MIDI-Wiedergabe)
- [x] Werkzeuge im Rechtsklick-Menü
- [ ] später: Spurhöhe ändern, Fades/Event-Lautstärke per Maus, Streaming langer Dateien von der Platte, bessere Resampling-Qualität

## ✅ Etappe 3 – Plug-ins
- [x] VST3-Hosting (Windows/macOS), Audio Units (macOS)
- [x] Plug-in-Suche im Hintergrund, automatisch beim ersten Start, Absturzschutz (abstürzende Plug-ins werden übersprungen), gespeicherte Liste
- [x] Browser zeigt Instrumente/Effekte nach Hersteller, Art oder als Favoriten (nach Benutzung, Rechtsklick entfernt), Knopf ↻
- [x] Instrumentenspuren spielen MIDI → Instrument (inkl. Transponieren/Anschlag), Note-Offs bei Stopp/Sprung/Loop
- [x] Insert-Ketten mit Bypass, Plug-ins laufen auch im Stopp weiter (Hall-Fahnen)
- [x] Plug-in-Fenster: „Öffnen“ im Inspector, Doppelklick / Menü auf einen Insert
- [x] Eingebauter „SM Testsynth“ (funktioniert ohne Plug-ins, für Tests)
- [x] Plug-in-Suche mit Fortschrittsfenster (DAW gesperrt, Abbrechen möglich), jedes Plug-in wird in eigenem Prozess geprüft (hängt es > 30 s oder stürzt es ab → übersprungen)
- [x] Plug-in-Fenster nie im Vollbild, zu große Editoren mit Bildlaufleisten; Plug-in-Fenster bleiben vor dem Hauptfenster; Instrument-Symbolknopf im Spurkopf
- [x] Inserts: Power-Knopf (Bypass), Öffnen-Knopf, Effekt auf belegten Platz ziehen = ersetzen
- [x] Live-MIDI: Keyboards und Bildschirm-Klaviatur (F7) spielen die ausgewählte Instrumentenspur
- [x] Menü „Einstellungen“: Audio (Soundkarte, Treiber inkl. ASIO, Abtastrate, Puffer), MIDI (Eingänge an/aus, Aktivitätsanzeige), Plug-ins, Farbschemata
- [x] Einstellungen im Cubase-Stil: Seitenleiste (Audio, MIDI, Metronom, Plug-ins, Darstellung) mit Tabs; MIDI-Anschlüsse-Tabelle (Aktiv, Anzeigename, Status), MIDI-Filter (Aufnahme/Thru), neue Geräte automatisch aktiv; Audiotreiber-Auswahl, Ausgänge/Eingänge; Farbschema-Kacheln, Oberflächengröße
- [x] Metronom: 5 Klänge, Lautstärke, Betonung, Vorzähler (1–4 Takte), Rechtsklick auf den Metronom-Knopf öffnet die Einstellungen
- [x] Plug-ins nur mit Haupt-Stereobus (Multi-Outs/Sidechain aus, z. B. Kontakt), Absturzbericht crash.log mit Symbolen (.pdb)
- [x] „Plug-ins suchen“ prüft alles neu, „Aktualisieren“ (↻) nur neue
- [x] Soundkarten-Wechsel ohne Absturz/Knacksen (Plug-ins werden außerhalb des Audio-Threads neu vorbereitet)
- [ ] später: Latenzausgleich (PDC), Plug-in-Zustand im Projekt speichern (mit Etappe 8),
      Plug-in-Parameter-Automation (Etappe 6), Inserts per Drag & Drop umsortieren

## Etappe 4 – Piano Roll
- [x] Editor unten (F2, Doppelklick auf einen MIDI-Part), wechselt sich mit der Konsole (F3) ab
- [x] Noten setzen (Doppelklick / Stift), verschieben (Raster, Alt = frei, Strg = kopieren), Länge ändern,
      löschen (Doppelklick, Entf, Radiergummi), Gummiband-Auswahl, Pfeiltasten (Umschalt = Oktave)
- [x] Anschlag-Spur (Velocity) unten, Klaviatur zum Vorhören, Quantisieren (Q) mit eigenem Raster
- [x] Zoom: Strg+Rad horizontal, Alt+Rad Notenhöhe
- [ ] Arpeggiator und MIDI-Effekte im Inspector
- [x] Neuer MIDI-Part: Stift im Arrangement oder Doppelklick in eine leere Instrumentenspur (öffnet den Editor)

## Etappe 5 – Konsole
- [ ] Busse, Sends, Ausgänge, Pegelanzeigen pro Kanal

## Etappe 6 – Automation
- [ ] Automationsspuren für Mixer- und Plug-in-Parameter

## Etappe 7 – Aufnahme
- [ ] Audio- und MIDI-Aufnahme, Monitoring, Metronom-Vorzähler, ASIO unter Windows

## Etappe 8 – Projekte & Export
- [ ] Startseite beim Programmstart (wie der Hub in Cubase 14): Neues Projekt (mit Vorlagen), Projekt öffnen,
      zuletzt geöffnete Projekte, Setup (Audio/MIDI)
- [ ] Speichern/Laden (Projektdatei + Audio-Pool, Plug-in-Zustände), Autosave, zuletzt geöffnete Projekte
- [ ] Export als WAV/FLAC/MP3, Einstellungen (Audiogerät, MIDI)

## Etappe 9 – ARA
- [ ] ARA-2-Host (Melodyne & Co.), Editor-Bereich für ARA-Plug-ins

## Danach
- Erweiterungen als DLL/dylib (C-ABI), Mehrsprachigkeit, Tempospur, Arranger-Spur,
  Comping, Ordnerspuren, Video …
