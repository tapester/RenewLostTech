# Maßnahmenplan RenewLostTech

Stand: 2026-11-11 · Analysebasis: Commit `4487bde`, abgeglichen mit `b3b5644` (`main`) · Plug-in-Version 0.2.6

Dieses Dokument fasst die Befunde aus dem Code-Review, die Refactoring-Optionen und die Upgrade-Optionen zu einer priorisierten Maßnahmenliste zusammen. Abschnitt 4 enthält Quick Wins, die sofort und ohne Änderung des Audioverhaltens umgesetzt werden können.

**Abgleich mit `b3b5644`:** Nach der Analyse hat `main` mehrere Punkte bereits umgesetzt (per Verzeichnis-Diff festgestellt, nicht gebaut): `logs/` und `legacy/` sind entfernt (QW-2, QW-11), `Documentation/html` und `tutorial/` von VSTGUI sind entfernt (QW-5 teilweise; `doxygen/` sowie `Documentation/index.html` und die RTF-Datei sind noch vorhanden), `CMakeLists.txt` setzt `cmake_minimum_required(VERSION 4.2)` und C++17 (QW-10), `WINVER`/`_WIN32_WINNT` stehen auf `0x0601` statt `0x0501` (in `wpstdinclude.h` bleibt ein Fallback `0x0501` hinter `#ifndef WINVER`), es gibt ein `windows.yml` (Windows-CI auf `windows-2022`) und ein macOS-Preset in `CMakePresets.json`. Betroffene Zeilen in Abschnitt 2 und 4 sind markiert. Neu offen: der README-Satz zum "legacy MinGW makefile" (Z. 5) verweist auf eine gelöschte Datei; in `windows.yml` sind `lukka/get-cmake@latest` und `ilammy/msvc-dev-cmd@v1` nicht auf feste Versionen gepinnt.

**Grenzen dieser Analyse:** Die Befunde stammen aus dem Lesen des Quelltexts. Es wurde nichts gebaut, profiliert oder in einem Host getestet. Einzelne Änderungen wurden nur per `g++ -fsyntax-only` auf einer Scratch-Kopie geprüft (siehe Abschnitt 6). Aufwände sind grobe Schätzungen.

## 1. Prioritätslogik

| Stufe | Bedeutung |
|---|---|
| **Gate** | Muss vor jeder Weitergabe von Quellen oder Binärdateien geklärt sein. Läuft parallel zur Entwicklung. |
| **Schritt 0** | Sicherheitsnetz. Muss stehen, bevor DSP- oder Threading-Code geändert wird. |
| **Kritisch** | Falsches Verhalten oder Echtzeit-Verletzung im laufenden Betrieb. |
| **Hoch** | Absturz- oder Speicherfehler-Risiko bei ungewöhnlichen Hostaufrufen oder Speicherknappheit. |
| **Mittel** | Qualität, Wartbarkeit, Testabdeckung, Build. |
| **Niedrig** | Aufräumen, Verteilung, strategische Entscheidung. |
| **QW** | Quick Win: höchstens etwa eine Stunde, keine Änderung des Audioverhaltens, durch Build, `ctest` oder pluginval prüfbar. |

Aufwand: **S** unter einem halben Tag, **M** ein bis drei Tage, **L** mehr als drei Tage.

## 2. Konsolidierte Maßnahmenliste

| ID | Maßnahme | Prio | Aufwand | Abhängig von | Abnahmekriterium |
|---|---|---|---|---|---|
| G1 | SDK-Herkunft und Lizenz klären und dokumentieren | Gate | S–M | – | `THIRD_PARTY.md` nennt Herkunft, Version und Lizenz; Entscheidung zu `aeffect.h`/`aeffectx.h` im öffentlichen Repo getroffen |
| S0 | Regressionsnetz: Golden-Render-Test plus Komponententests | Schritt 0 | M | – | `ctest` prüft Offline-Render gegen Referenz (Toleranz) und Analyzer/Synthesizer/BufferManager; grün unter Windows und macOS |
| K1 | Sample-Rate-Zustand pro Plug-in-Instanz | Kritisch | M | S0 | Zwei Instanzen mit 44,1 und 96 kHz liefern parallel dasselbe wie einzeln |
| K2 | Audio-Thread ohne Sperren | Kritisch | M–L | S0, K1 | Kein Lock im Audio-Pfad; Stresstest mit parallelem `setParameter`/GUI ohne Aussetzer |
| K3 | Keine Allokation im Audio-Thread (Vorallokation) | Kritisch | M | S0, K1 | Änderung von `BufrSize` oder Sample-Rate allokiert nicht im Audio-Callback |
| H1 | Parameterindex und Wertebereich absichern | Hoch | S | S0 | Tests mit Index -1 und 54 sowie Werten -1, 2, NaN laufen ohne Absturz |
| H2 | Allokations- und Rollback-Pfade, Buffer-Besitz | Hoch | M | K3 | Fehlgeschlagener Puffer-Wechsel lässt Analyzer und Synthesizer im alten, konsistenten Zustand |
| H3 | VST2-String-Längen einhalten | Hoch | S | – | Keine ungebundenen `strcpy`/`sprintf` in Host-Callbacks |
| M1 | Editor-Lebenszyklus testen (Fix beibehalten) | Mittel | S–M | S0 | Mehrfaches Öffnen/Schließen und Zerstörung bei offenem Editor ohne Absturz (pluginval GUI) |
| M2 | Testumfang erweitern | Mittel | M | S0, K1, H1, H2 | Tests für mehrere Instanzen, ungültige Indizes, Allokationsfehler (Injektion), Editor-Lifecycle |
| M3 | Parameter-Deskriptor-Tabelle | Mittel | M | S0, K1 | Eine Tabelle ersetzt die sechs parallelen Arrays; Kanal wird explizit übergeben; Abgleich mit `parameters.md` |
| M4 | `WavePlug.cpp` nach Verantwortung aufteilen | Mittel | S–M | M3 | Dateien für Host-Callbacks, Zustand/Thread-Austausch, Verarbeitung; öffentliche API unverändert |
| M5 | Build und CI modernisieren | Mittel | M | – | Windows-CI vorhanden, Versionen gepinnt, pluginval auf macOS verbindlich |
| N1 | Code-Hygiene (RAII, `constexpr` statt Makros) | Niedrig | M | M3 | Keine rohen `new`/`delete` in DSP-Code |
| N2 | Verteilung (Developer ID, Notarisierung, Windows-Signatur) | Niedrig | M | G1 | Binärdateien laufen auf fremden Rechnern ohne Quarantäne-Workaround |
| E1 | Entscheidung zu Framework und Migration | Niedrig (strategisch) | – | K1–K3, H2 | Entscheidung für Option A–D (Abschnitt 5) |

### Details und Fundstellen

**G1 – SDK-Herkunft und Lizenz.** `THIRD_PARTY.md` weist die ungeklärte Herkunft des VST-2.4-SDK selbst aus. Laut Steinbergs Lizenz-FAQ dürfen VST2-Quellen auf GitHub geteilt werden, aber ohne `aeffect.h` und `aeffectx.h`, weil diese nicht weiterverteilt werden dürfen. VST2-Binärdateien darf nur weitergeben, wer die Lizenzvereinbarung vor Oktober 2018 unterschrieben hat. Beide Header liegen unter `dependencies/vstsdk2.4/public.sdk/source/vst2.x/` im Repo. Das ist ein Hinweis, keine Rechtsberatung.

**S0 – Regressionsnetz.** `tests/dsp/wpfunc_tests.cpp` linkt nur `src/dsp/wpfunc.cpp`. Analyzer, Synthesizer, Modulatoren und BufferManager sind ungetestet. `BufferManager` ist eine konkrete Klasse; Analyzer und Synthesizer lassen sich damit isoliert instanziieren (Sample-Rate vorab über `setGlobalSampleRate`). Vorschläge: Analyzer erkennt 440 Hz innerhalb einer Toleranz; Synthesizer-Periode passt zur gesetzten Frequenz; Glättungsfenster; Puffergrößenwechsel; `deleteFloatBuffer` auf unbekannten Zeiger liefert `false`. Ein End-to-End-Test über `WavePlug::processReplacing` mit Stub-`audioMaster` ist denkbar, wurde aber nicht ausprobiert.

**K1 – Sample-Rate pro Instanz.** `globalSampleRate`, `globalMaxFrequency` und `globalMinPeriodLength` sind Prozess-Globals (`src/dsp/wpfunc.cpp`, Z. 24–28). Verwendet werden sie in `Analyzer.cpp` (Z. 63, 64, 124, 125, 129, 130, 188), `Synthesizer.cpp` (Z. 70, 166), `wpmodulators.cpp` (Z. 56) und `WavePlug.cpp` (u. a. Z. 78, 145, 221, 713–725, 816–817). Der Audio-Thread schreibt sie über `setGlobalSampleRate`. Mehrere Instanzen mit unterschiedlichen Sample-Raten stören sich dadurch gegenseitig. Ziel: ein Kontext pro Instanz, der an Analyzer, Synthesizer, Modulatoren und Parameter-Mapping übergeben wird.

**K2 – Audio-Thread ohne Sperren.** `processReplacing` ruft `doThreadSynchronizedDataExchange()` auf (`WavePlug.cpp` ab Z. 670). Dort wird ein Lock genommen und die komplette `sharedData`-Struktur kopiert. Derselbe Lock (`myCriticalSection`, definiert in `BufferManager`) wird von Host-Aufrufen (`setParameter`, `getParameter`, Programmnamen) und der GUI (`getAmplitude`, `getFrequency`) gehalten. Ziel: atomare Parameterwerte und atomare Monitorwerte; Hilfetexte und Programmname außerhalb des Audio-Pfads.

**K3 – Vorallokation.** `doParameterUpdates` → `setBufferSizeMultiplier` (`WavePlug.cpp` ab Z. 815) → `Analyzer::setBufferSize` (`Analyzer.cpp` ab Z. 79) und die Synthesizer-Entsprechung rufen `new[]` im Audio-Thread auf, sobald `BufrSize` oder die Sample-Rate wechselt. Ziel: im `resume()` bzw. `setSampleRate` auf das benötigte Maximum vorallokieren.

**H1 – Parameter absichern.** `setParameter` (`WavePlug.cpp` ab Z. 405) schreibt `sharedData.newParamValues[index]` und `paramHelpTexts[index+1]` ohne Indexprüfung. `getParameter` (ab Z. 446) liest `sharedData.paramValues[index]` ungeprüft. Eingehende Werte werden nicht auf 0..1 begrenzt; `U_MOD_TYPE_T(value)` und verwandte Makros indizieren damit schon vor dem Lock die Hilfetext-Tabellen. `NaN` ist intern der Marker für "nicht aktualisiert" und sollte als Eingabe bewusst behandelt werden.

**H2 – Allokation, Rollback, Besitz.** In `BufferManager::newIntBuffer` und `newFloatBuffer` steht `push_back` (Z. 89 und 109) außerhalb des `try`-Blocks: eine Ausnahme leckt den Puffer und läuft in den Host. `Analyzer::setBufferSize` löscht die alten Puffer, bevor die neuen allokiert sind; der "Rollback" in `WavePlug::setBufferSizeMultiplier` allokiert die alte Größe neu, der Inhalt ist verloren, und das kann erneut scheitern (dann `operational = false`). Der Synthesizer wurde nicht vollständig gelesen und ist vermutlich analog. Ziel: erst neu allokieren, dann tauschen; `BufferManager` durch Member (`std::vector`/`unique_ptr`) in Analyzer und Synthesizer ersetzen, sodass der Lock nur noch den Plug-in-Zustand schützt. Allokationsstellen bleiben dieselben, im Audio-Pfad kommt keine hinzu.

**H3 – String-Längen.** `kVstMaxParamStrLen` ist 8 (`aeffect.h`, Z. 284). `fOffsetDisplayer` kann bis `-22000.00` ausgeben (9 Zeichen); `fMinMaxDisplayer` bis `22000.00` (8 Zeichen). Die Displayer (`WavePlug.cpp`, Z. 138–240) und `getParameterDisplay` nutzen `strcpy`/`sprintf` ohne Grenze. Die Parameternamen (z. B. `AModTyp1`) passen mit genau 8 Zeichen. `setProgramName` kopiert per `strcpy` in `programName[32]`, der Host-Vertrag erlaubt 24 Zeichen.

**M1/M2 – Lebenszyklus und Tests.** Der Fix in `WavePlugEditor::close()` (`WavePlugEditor.cpp` ab Z. 411) bleibt unverändert. Zu prüfen sind mehrfaches Öffnen/Schließen und die Zerstörung des Plug-ins bei offenem Editor. Ergänzende Tests: mehrere Instanzen mit verschiedenen Sample-Raten, ungültige Parameterindizes, Allokationsfehler (dafür braucht der BufferManager bzw. sein Nachfolger einen Injektionspunkt).

**M3 – Parameter-Deskriptor.** In `WavePlugParameters.cpp` stehen sechs parallele Arrays mit je `[kNumParams]` (Namen, Labels, Hilfetexte, Defaults, Anzeigefunktionen, Setter). Alle haben aktuell 26 Einträge (geprüft), es gibt also keinen bestehenden Fehler. Fehlende Initialisierer würden aber stillschweigend zu `nullptr` aufgefüllt. Die Setter wirken über die Zeiger `anaE`/`modAE`/`modFE`/`modWE`/`synE`/`modOE`, die `setEditMode` auf Kanal 1 oder 2 setzt. Ziel: ein Deskriptor pro Parameter (Name, Label, Default, Hilfetext, Anzeige, Setter, Kanal) mit expliziter Kanalreferenz statt `editMode`. Dieselbe Tabelle kann `parameters.md` prüfen und später VST3-, CLAP- oder JUCE-Parameter erzeugen. Die dynamischen Modulator-Hilfetexte bleiben getrennt.

**M4 – Aufteilung `WavePlug.cpp`.** Bestehende Blöcke: Konvertierungsmakros (Z. 70–100), Handler-Tabellen (ab Z. 101), Displayer (138–240), Host-Callbacks (329–580), Zugriffsfunktionen (581–650), Thread-Austausch, Re-Init und Setter (670–908), Verarbeitungs-Handler. Vorschlag: Mapping-Header (nach M3 überflüssig), Host-Callbacks, Zustand/Austausch, Verarbeitung. Die `*_T`-Makros nutzen `globalMaxFrequency` und müssen vorher in Funktionen mit explizitem Kontext (K1) überführt sein.

**M5 – Build und CI.**
- Erledigt in `b3b5644`: `cmake_minimum_required(VERSION 4.2)` passt jetzt zu `CMakePresets.json`, `CMAKE_CXX_STANDARD 17` ist gesetzt.
- `WINVER`/`_WIN32_WINNT` stehen in `CMakeLists.txt` jetzt auf `0x0601` (Windows 7). Offen: der Fallback `0x0501` in `wpstdinclude.h` (Z. 5–6, hinter `#ifndef WINVER`) und die Frage, ob die GUI-Variante (VSTGUI 3.5) mit `0x0601` unverändert baut.
- Erledigt in `b3b5644`: `windows.yml` (Windows-CI, `windows-2022`, baut, testet, packt die DLLs). Offen: Actions-Versionen pinnen (`lukka/get-cmake@latest`, `ilammy/msvc-dev-cmd@v1`), pluginval in der Windows-CI ergänzen.
- `macos-latest` und das pluginval-Release `latest` sind nicht gepinnt; die pluginval-Stufe läuft mit `continue-on-error`.
- `ScopedFtz` nutzt für arm64 GCC/Clang-Inline-Assembler; das baut nicht mit MSVC für Windows ARM64 (nur relevant, falls dieses Ziel gewünscht ist).

**N1 – Code-Hygiene.** Rohes `new`/`delete` (Editor, BufferManager), Konvertierungsmakros als `constexpr`-Funktionen. Der Rest steckt in den Quick Wins.

**N2 – Verteilung.** macOS ist nur ad-hoc signiert (README). Für andere Rechner sind Developer ID und Notarisierung nötig, für Windows eine Signatur. Hängt an G1.

## 3. Reihenfolge

1. **Phase 0 (sofort, parallel):** Quick Wins aus Abschnitt 4; G1 starten.
2. **Phase 1:** S0 (Regressionsnetz). Vorher QW-1 (NoGUI-Build), damit die Tests gegen beide Varianten laufen.
3. **Phase 2:** K1, K2, K3.
4. **Phase 3:** H1, H2 (zusammen mit K3), H3, sofern nicht als Quick Win erledigt.
5. **Phase 4:** M1, M2, M3, danach M4; M5 unabhängig dazwischen.
6. **Phase 5:** E1 (Entscheidung), N1, N2.

G1 muss vor jeder Weitergabe abgeschlossen sein, unabhängig von der Phase.

## 4. Quick Wins

Kriterien: höchstens etwa eine Stunde, keine Änderung des Audioverhaltens, durch Build, `ctest` oder pluginval prüfbar. **Ein Commit pro Quick Win.**

| ID | Maßnahme | Dateien | Prüfung | Risiko |
|---|---|---|---|---|
| QW-1 | NoGUI-Build explizit machen: `WavePlug.cpp` und `WavePlugMain.cpp` direkt in `NOGUI_SOURCE_FILES` aufnehmen, `target_compile_definitions(LostTechNoGUI PRIVATE WP_NO_GUI)` setzen, `WavePlugNoGUI.cpp` und `WavePlugMainNoGUI.cpp` löschen, Kommentar in `CMakeLists.txt` (Z. 58) anpassen. Hinweis: `WavePlugMain.cpp` verwendet `WP_NO_GUI` gar nicht, der Main-Wrapper war wirkungslos | `CMakeLists.txt`, `src/plugin/` | Vier Builds (Debug/Release, mit/ohne GUI), pluginval, macOS-CI | sehr niedrig |
| QW-2 (**erledigt**, `b3b5644`) | `logs/` aus dem Git-Index nehmen (`git rm -r --cached logs`). Die Dateien stehen in `.gitignore`, sind aber committet und enthalten einen lokalen Windows-Pfad mit Benutzername. Die Historie behält die Datei; bei Bedarf separat bereinigen | `logs/` | `git ls-files logs` ist leer | sehr niedrig |
| QW-3 | Tabellengrößen zur Compile-Zeit prüfen: in `WavePlug.hpp` die sechs Arrays ohne Grenze deklarieren (`[]`), in `WavePlugParameters.cpp` die Definitionen ebenfalls und eine statische Funktion `verifyTables()` mit sechs `static_assert(sizeof(a)/sizeof(a[0]) == kNumParams)` ergänzen. Auf Scratch-Kopie mit `g++ -fsyntax-only` geprüft: fehlt ein Eintrag, schlägt der Build mit "static assertion failed" fehl | `WavePlug.hpp`, `WavePlugParameters.cpp` | Build; absichtlich einen Eintrag entfernen | sehr niedrig |
| QW-4 | `catch` per `const &` statt per Wert (6 Stellen: `WavePlugMain.cpp` Z. 66 und 69, `WavePlug.cpp` Z. 306, `BufferManager.cpp` Z. 45, 83, 103). Dazu falschen Kommentar in `BufferManager.cpp` Z. 45 korrigieren ("Editor allocation failed" steht dort bei den Puffer-Vektoren) | siehe links | Build, `ctest` | sehr niedrig |
| QW-5 (**teilweise erledigt**: `Documentation/html` und `tutorial/` weg, `doxygen/` offen) | Ballast entfernen: `dependencies/vstsdk2.4/vstgui.sf/vstgui/Documentation/` (242 Dateien, 8,1 MB), `tutorial/` (144 KB), `doxygen/` (56 KB). Von CMake nicht referenziert. **Vorher** den VSTGUI-Lizenztext aus den Header-Kommentaren (`vstgui.h`, `aeffguieditor.h`) in `THIRD_PARTY.md` übernehmen, weil `Documentation/html/license.html` sonst die einzige Kopie wäre | `dependencies/…/vstgui` | Alle Builds, pluginval | niedrig |
| QW-6 | H3 umsetzen: in den Displayern `snprintf(text, kVstMaxParamStrLen + 1, …)` bzw. `vst_strncpy` statt `sprintf`/`strcpy`; `setProgramName` auf `kVstMaxProgNameLen` begrenzen. Anzeige kann bei `-22000.00` um die letzte Nachkommastelle gekürzt werden | `WavePlug.cpp` (Z. 138–240, 374ff, 473ff) | Build, pluginval, Sichtprüfung der Anzeigen | niedrig |
| QW-7 | CI härten: `permissions: contents: read`, `concurrency` mit `cancel-in-progress`, `timeout-minutes`, feste Runner-Version statt `macos-latest`, festes pluginval-Release statt `latest/download` (aktuelle Runner-Versionen in der GitHub-Doku prüfen) | `.github/workflows/macos.yml` | Workflow-Lauf | niedrig |
| QW-8 | `THIRD_PARTY.md` ergänzen: pluginval-Version eintragen (Platzhalter in Z. 19; Ausgabe von `pluginval.exe --version` nötig), VSTGUI-Lizenztext (zusammen mit QW-5), Hinweis auf die VST2-Header-Lizenzlage (G1) | `THIRD_PARTY.md` | Durchsicht | keins |
| QW-9 | `.gitattributes` (`* text=auto`, `*.bmp binary`) und `.editorconfig` hinzufügen. Aktuell sind die geprüften Dateien LF, es gibt weder `.gitattributes` noch `.editorconfig`; das verhindert Zeilenende-Drift bei Arbeit unter Windows | Repo-Wurzel | `git status` nach Checkout sauber | sehr niedrig |
| QW-10 (**erledigt**, `b3b5644`) | C++-Standard explizit setzen: `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`. Die Quellen der No-GUI-Variante und der Test lassen sich unter `g++ -std=c++17 -fsyntax-only` ohne Fehler übersetzen; MSVC und die GUI-Variante sind nicht geprüft | `CMakeLists.txt` | Alle Builds | niedrig |
| QW-11 (**Datei erledigt**, README-Satz offen) | Optional, Entscheidung des Eigentümers: `legacy/makefile.mingw` entfernen (laut README nicht gepflegt; bleibt in der Git-Historie) und die README-Zeile anpassen | `legacy/`, `README.md` | Durchsicht | sehr niedrig |

**Bewusst keine Quick Wins:** K1–K3, H1, H2, M3 und M4. Sie ändern Zustandsverwaltung, Threading oder Parameterfluss und gehören hinter das Regressionsnetz S0.

## 5. Upgrade-Optionen (Entscheidung E1)

Voraussetzung für jede Option: den DSP-Kern (`src/dsp`, etwa 1.600 Zeilen) als frameworkfreie Bibliothek mit Instanz-Kontext herauslösen (K1) und M3 umsetzen. Diese Arbeit ist in jeder Option nötig und geht nicht verloren.

| | **A: JUCE (8/9)** | **B: VST3 SDK 3.8.x + VSTGUI 4** | **C: CLAP-first + clap-wrapper** | **D: VST2 behalten** |
|---|---|---|---|---|
| Formate | VST3, AU, Standalone; CLAP nur über Zusatzprojekt | VST3 (AU/CLAP über Wrapper) | CLAP, daraus VST3/AU/Standalone¹ | nur VST2 |
| macOS-GUI (arm64) | ja, GUI in JUCE neu bauen | ja, GUI auf VSTGUI 4 portieren | ja, eigene GUI nötig | nein (VSTGUI 3.5 braucht Carbon) |
| Lizenz | AGPLv3 oder kommerziell² | MIT | MIT-nah¹ | G1-Problem bleibt |
| Aufwand | mittel | mittel bis hoch | hoch | niedrig |
| Passt zu | schnellster Weg zur Mac-GUI | nächste Verwandtschaft zum jetzigen Aufbau | CLAP-Mapping in `parameters.md` | reine Pflege |

¹ Aus dem Gedächtnis, nicht gegen aktuelle Dokumentation geprüft. ² Lizenzbedingungen vor der Entscheidung aktuell prüfen.

Belegt (Stand der Recherche am 2026-11-11):
- Das VST3 SDK steht unter MIT-Lizenz. Version 3.8.1 erschien am 11. August 2026 und enthält VSTGUI 4.15.1.
- JUCE 9 hat unveränderte Preise und Bedingungen gegenüber JUCE 8. CLAP wird in der Ankündigung nur als künftiges Ziel genannt.
- JUCE liefert seit 2018 kein VST2-SDK mehr mit.

**Empfehlung:** Nach S0 und K1–K3 entscheiden. Option A, wenn schnell eine macOS-GUI gewünscht ist und ohnehin JUCE genutzt wird; Option B, wenn eine MIT-saubere Lösung nahe am bisherigen Aufbau Vorrang hat. Option D löst G1 nicht. In allen Fällen die VST2-Version auf einem eigenen Branch einfrieren.

## 6. Offene Punkte und Verifikation

- Nicht gebaut, nicht profiliert, nicht in Hosts getestet. Die Befunde zu Threading und Allokation beruhen auf dem Quelltext.
- Der Synthesizer-Rollback (H2) wurde nicht vollständig gelesen.
- Geprüft auf einer Scratch-Kopie mit `g++ -std=c++17 -fsyntax-only -D__cdecl=` (Linux): die `static_assert`-Variante aus QW-3 (greift bei fehlendem Eintrag, kompiliert bei vollständigen Tabellen und mit geändertem Header auch `WavePlug.cpp`) und der C++17-Syntaxcheck aus QW-10 für DSP, Plug-in-Quellen ohne GUI, Test und SDK-Quellen.
- Nicht geprüft: MSVC-Build, GUI-Variante, macOS-Build der Änderungen.
- Details zu clap-wrapper und die aktuellen JUCE-Lizenzbedingungen wurden nicht abgerufen.
- G1 ist keine Rechtsberatung.

## 7. Quellen

- [VST 3 Licensing FAQ (Steinberg)](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Licensing.html)
- [VST 3.8.1 SDK Released](https://forums.steinberg.net/t/vst-3-8-1-sdk-released/1038865)
- [JUCE 9 is Available Now](https://forum.juce.com/t/juce-9-is-available-now/69175)
- [The removal of JUCE's embedded VST2 SDK](https://forum.juce.com/t/the-removal-of-juce-s-embedded-vst2-sdk/29994?page=2)
