# Hinweise für die Arbeit an SM Studio

Lineare DAW in C++20 mit JUCE 9 (AGPLv3). Ziel: Bedienung ähnlich Studio One 3, kostenlos und Open Source,
Windows + macOS. Der Projektinhaber spricht Deutsch – Antworten und UI-Texte auf Deutsch, Code/Kommentare Englisch.

Lies zuerst `docs/ARCHITECTURE.md` (Aufbau, Regeln) und `docs/ROADMAP.md` (was als Nächstes dran ist).

## Bauen und testen (Linux-Container)

```bash
# einmalig: JUCE-Abhängigkeiten (siehe .github/workflows/build.yml) per apt installieren
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/tests/SMStudioTests_artefacts/Release/SMStudioTests
xvfb-run -a "build/src/app/SMStudio_artefacts/Release/SM Studio" --screenshot /tmp/shot.png --show console
```

Mit einem lokalen JUCE-Checkout: `-DFETCHCONTENT_SOURCE_DIR_JUCE=/pfad/zu/JUCE`.
Den Screenshot nach UI-Änderungen immer ansehen. `docs/screenshot.png` bei sichtbaren Änderungen erneuern.

## Regeln

- Song-Änderungen nur über `Command`s (`ctx.perform`), nie direkt – sonst kein Undo.
- Neue Funktionen als Modul (`src/app/modules/`), registriert über die `ExtensionRegistry`.
- Audio-Thread: keine Locks, keine Allokation, kein Zugriff auf `Song`.
- Umlaute nur in `u8"…"`-Literalen; Verketten mit `juce::String (u8"…") + x`.
- Neue Core-Logik bekommt Unit-Tests in `tests/`.
- Kein Code aus Lemon Studio oder anderen GPL-Projekten übernehmen (nur Konzepte nachschlagen).
- Keine Grafiken, Logos oder Namen von PreSonus/Studio One verwenden.
- Größere Etappen auf eigenem Branch + Pull Request; Roadmap-Häkchen setzen.
