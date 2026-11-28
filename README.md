# SM Studio

Eine kostenlose, quelloffene, **lineare** Digital Audio Workstation für Windows und macOS –
im Bedienkonzept angelehnt an klassische lineare DAWs: Spuren, Events auf einer Zeitleiste,
Inspector links, Browser rechts, Konsole unten, Transportleiste unten.

> *A free, open-source, linear DAW for Windows and macOS, written in C++ with JUCE.*

**Status:** frühe Entwicklung (Etappe 3 – Plug-ins). Noch nicht für echte Produktionen geeignet.
Siehe [Roadmap](docs/ROADMAP.md).

![Screenshot](docs/screenshot.png)

## Was schon geht

- Fenster mit Toolbar, Inspector, Arrangement, Browser, Konsole und Transportleiste
- Audio- und Instrumentenspuren anlegen, umbenennen, löschen, stumm/solo, Lautstärke und Panorama
- Drag & Drop aus dem Browser: Instrument → neue Instrumentenspur, Effekt → Insert auf der Spur,
  Audiodatei → Audiospur mit Event (auch direkt aus Explorer/Finder)
- Lautstärke und Panorama per Fader/Regler oder als Zahl eintippen (z. B. `-3,5` oder `L30`)
- Konsole, Browser und Inspector abkoppelbar (eigenes Fenster, z. B. auf einem zweiten Bildschirm);
  Fensterpositionen und Layout werden gespeichert
- Rückgängig/Wiederherstellen für alle Änderungen
- Audio-Events abspielen (Lautstärke, Panorama, Stumm, Solo), Wellenformen, Loop-Bereich
- Events auswählen (auch per Rahmen), verschieben (auch auf andere Spuren), kopieren (Alt+Ziehen),
  Länge ändern, teilen, löschen, stummschalten, duplizieren; Spuren per Ziehen umsortieren
- Audioausgabe mit Transport (Play/Stop/Positionieren) und Metronom
- Menüs und Tastaturkürzel kommen vollständig aus dem Modulsystem

**Plug-ins:** Beim ersten Start sucht SM Studio im Hintergrund nach installierten VST3-Plug-ins
(am Mac auch Audio Units). Danach erneut über Browser → Instrumente/Effekte → „Plug-ins suchen“.
Instrument aus dem Browser ins Arrangement ziehen = neue Instrumentenspur; Effekt auf eine Spur oder
einen Kanal ziehen = Insert. Plug-in-Fenster: „Öffnen“ im Inspector bzw. Doppelklick auf einen Insert.
Ohne eigene Plug-ins kann man mit dem eingebauten „SM Testsynth“ testen.

**Maus im Arrangement:** Event anklicken = auswählen (Strg/Cmd oder Umschalt: hinzufügen),
ziehen = verschieben, Alt+Ziehen = kopieren, Ränder ziehen = Länge ändern, Umschalt beim Ziehen = ohne Raster.
Leere Fläche ziehen = Auswahlrahmen. Lineal oben ziehen = Loop-Bereich, unten klicken = Abspielposition.
Spurkopf hoch/runter ziehen = Spur umsortieren. Rechtsklick auf Events = Menü (teilen, zusammenfügen, kopieren …).
Konsole, Inspector und Browser lassen sich an ihrem Rand stufenlos vergrößern. Das Raster „Adaptiv“ passt sich dem Zoom an.
Im Browser unter „Dateien“ navigiert man wie im Explorer/Finder durch die Ordner (Musik, Desktop, Laufwerke);
geöffnete Ordner werden beim nächsten Start wiederhergestellt. Rechtsklick im Arrangement zeigt oben die Werkzeuge.
Der Inspector zeigt für die ausgewählte Spur einen kompletten Kanalzug (Inserts, Sends, Panorama, Fader),
bei Instrumentenspuren darüber die Instrument-Einstellungen (Transponieren, Anschlag; Arpeggiator und MIDI-FX folgen).

## Tastaturkürzel

| Taste | Funktion |
|---|---|
| Leertaste | Wiedergabe / Stopp |
| Pos1 oder `,` | Zum Anfang |
| ← / → | Einen Takt zurück / vor |
| C | Metronom |
| L oder / | Loop ein/aus |
| P | Loop auf ausgewählte Events setzen |
| Entf | Ausgewählte Events löschen (sonst die ausgewählte Spur) |
| D | Ausgewählte Events duplizieren |
| Alt+X | Am Abspielpunkt teilen |
| G | Ausgewählte Events zusammenfügen |
| Strg/Cmd+X / C / V | Ausschneiden / Kopieren / Einfügen (am Abspielpunkt) |
| Umschalt+M | Events stumm ein/aus |
| Strg/Cmd+A, Esc | Alle Events auswählen / Auswahl aufheben |
| T / Umschalt+T | Audio- / Instrumentenspur hinzufügen |
| ↑ / ↓ | Spur auswählen |
| M / S | Spur stumm / solo |
| 1 – 6 | Werkzeuge: Pfeil, Bereich, Teilen, Radiergummi, Stift, Stumm |
| Strg/Cmd+Z, Strg/Cmd+Umschalt+Z | Rückgängig / Wiederherstellen |
| F3 / F4 / F5 | Konsole / Inspector / Browser ein-/ausblenden |
| Strg/Cmd + Mausrad | Zoom |

## Herunterladen

Fertige Programme entstehen bei jedem Push automatisch:
**Actions** → neuester Lauf von „Build“ → unten unter *Artifacts*:

- `SM-Studio-Windows` – enthält `SM Studio.exe`
- `SM-Studio-macOS` – enthält `SM Studio.app.zip` (Universal: Apple Silicon und Intel)

**macOS-Hinweis:** Die App ist (noch) nicht von Apple notarisiert. Beim ersten Start
Rechtsklick → *Öffnen* wählen, oder im Terminal: `xattr -cr "/Applications/SM Studio.app"`.

## Selbst bauen

Voraussetzungen: CMake ≥ 3.22, ein C++20-Compiler (Visual Studio 2022, Xcode 15+ oder GCC 13+).
JUCE wird beim Konfigurieren automatisch heruntergeladen.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release        # Unit-Tests
```

Unter Linux werden zusätzlich die JUCE-Abhängigkeiten benötigt (siehe `.github/workflows/build.yml`).

## Mitentwickeln

Wie das Programm aufgebaut ist und wie man neue Funktionen als Modul ergänzt, steht in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Lizenz

SM Studio steht unter der **GNU Affero General Public License v3** (siehe [LICENSE.md](LICENSE.md)).
Es verwendet das [JUCE](https://juce.com)-Framework unter dessen AGPLv3-Lizenz.
„VST“ ist eine Marke der Steinberg Media Technologies GmbH. SM Studio steht in keiner
Verbindung zu PreSonus, Fender oder Steinberg.
