# Spektra – Design

Stand: 2026-10-26 · Status: Entwurf zur Freigabe

## 1. Ziel

Spektra ist ein eigenständiges Kodi-Add-on, das Musik in audio-reaktive Animationen
übersetzt. Es enthält **Szenen**; die erste Szene heißt **Eclipse**: eine digitale
Sonnenfinsternis, deren Ring aus feinem Pixelstaub brennt, Wellen abgibt und eine
warme Corona trägt. Weitere Szenen sollen später ohne Umbau hinzukommen.

Spektra verhält sich bei Musikwiedergabe wie Party Video (öffnet die Visualisierung
selbst, kehrt nach Bedienpausen zurück, lässt sich unterdrücken) und bietet eine
JSON-RPC-Schnittstelle, damit der KodiMediaBot es später steuern kann.

**Erfolgskriterium:** Auf dem Pi 5 sieht und verhält sich Eclipse wie der Prototyp
`docs/entwuerfe/dune-v14-favorit.html` (Referenz), mit allen dortigen Reglern als
Kodi-Einstellungen und denselben Standardwerten, und läuft flüssig (Ziel ≥ 30 fps
bei 1080p mit Standardeinstellungen).

## 2. Rahmen

| | |
|---|---|
| Gerät | Raspberry Pi 5 (aarch64), LibreELEC 12, Kodi 21 „Omega“ |
| Add-on-ID | `visualization.spektra`, Name „Spektra“, Version 0.1.0 |
| Sprachen | C++17 + OpenGL ES 3.0 (Renderer), Python 3.11 (Service, Befehle) |
| Repo | `~/githubprojects/visualization.spektra` |
| Abhängigkeiten | keine zu Party Video; benötigte Teile werden kopiert und angepasst |

Regeln wie bei Party Video: Version steht nur in `addon.xml.in`; nur `kodi.py`, `ui.py`,
`default.py`, `service.py` importieren `xbmc*`; ruff mit `target-version = "py311"`;
auf dem Pi nur lesender Zugriff (Logs, `addon_data`), Installation macht der Maintainer.

## 3. Übernahme aus Party Video

| Teil | Übernahme | Anpassung |
|---|---|---|
| `build.sh`, `test.sh`, `docker/`, `scripts/in-container.sh`, `build-in-container.sh`, `check_elf.sh`, `check_zip.sh`, `test-cpp-in-container.sh`, `tests/build/*` | kopieren | Namen, keine FFmpeg-Abhängigkeit |
| `CMakeLists.txt`-Gerüst (`build_addon`, GLES3) | kopieren | Quellen, kein FFmpeg |
| `service.py`: automatisches Öffnen, Titelinfo ausblenden, Rückkehr nach 3 s Bedienpause, Modal-/Bildschirmschoner-Prüfung, `VisualisationRecovery` | kopieren | IDs, Bedingung „Spektra ist gewählt und eingeschaltet“ statt „Videoquelle gesetzt“, zusätzlich `partyvideo.suppress` beachten |
| `kodi.py` (`rpc`, `send`, Adapter), `statefile.py`, `commands.py` (Parser) | kopieren | neue Aktionen, kein YouTube |
| `default.py`, `ui.py` (Menü) | kopieren | neue Menüeinträge |
| Logging aus dem Renderer über Warteschlange (`LogQueue`) | kopieren | Präfix |
| Prozessweiter Zustand über Kodi-Instanzen hinweg | Muster übernehmen | Kodi behält die Instanz über Songwechsel, zerstört sie aber beim Schließen des Fensters und entlädt die .so (dlclose); die Szene läuft prozessweit weiter, die .so wird mit `-z nodelete` gelinkt |

Nicht übernommen: YouTube, yt-dlp/Deno, PO-Token, Video-Decoder, Download-Fortschritt.

## 4. Architektur

```
Kodi ──AudioData(PCM)──► AudioAnalyzer ──Features──► Scene (Eclipse) ──DrawList──► GL-Renderer ──► Bildschirm
                                 ▲                         ▲
                 Settings (Kodi + live.json) ──────────────┘
service.py ◄──Other.spektra_cmd── default.py ◄── Addons.ExecuteAddon (Bot/Menü)
    │ schreibt live.json, Settings, Status (Other.spektra_status, Window(Home).Property(spektra.*))
    └ öffnet/schließt das Visualisierungsfenster
Renderer ──renderer.json (Herzschlag, fps)──► service.py (Recovery)
```

### 4.1 C++-Einheiten

| Einheit | Aufgabe | Kodi/GL? |
|---|---|---|
| `addon.{h,cpp}` | `CInstanceVisualization`: `Start`, `Stop`, `AudioData`, `Render`, `IsDirty`; reicht an den prozessweiten `Engine` weiter | Kodi |
| `core/Fft` | reelle FFT (4096 Punkte; Blackman-Fenster wie der WebAudio-Analyser des Prototyps) | nein |
| `core/AudioAnalyzer` | Spektrum → vier Bänder, Pegel, Anschläge, Tonhöhe, Intensität, Drop, 32-Band-Feinspektrum (§5) | nein |
| `core/Settings` | Werte aller Einstellungen + Standardwerte + Lesen von `live.json` | nein |
| `core/Routing` | Zuordnung Band → Effekt; liefert `level(effect)`, `onset(effect)` | nein |
| `scene/Scene` | Schnittstelle: `Update(features, settings, dt)`, `Draw(DrawList&)`, `Resize(w,h)` | nein |
| `scene/eclipse/*` | Eclipse: Flammenfeld, Staub, Corona, Wellen, Strömungsfeld (§6) | nein |
| `gl/Renderer` | zeichnet eine `DrawList`: Staubpunkte, weiche Corona-Ebene, Linien/Ringe, Bloom, schwarzes Loch | GL |
| `core/StateFiles` | `live.json` lesen, `renderer.json` schreiben (atomar) | nein |

Szenen erzeugen nur eine **DrawList** (Staubpunkte mit Position/Farbe/Alpha/Größe,
Corona-Punkte, Kreise/Ringe mit Breite/Farbe, Bogensegmente, Bloom-Stärke, Lochradius).
Damit sind Simulation und Analyse ohne GPU testbar, und neue Szenen nutzen denselben
Renderer.

### 4.2 Threading und Lebensdauer

- `AudioData()` und `Render()` laufen in Kodi-Threads. Der Analyzer puffert Samples
  (Mono-Mix) in einem Ringpuffer; die Analyse läuft in `Render()` einmal pro Bild auf
  den neuesten 4096 Samples. Kein eigener Worker-Thread nötig.
- `Engine` (Analyzer, Szene, Settings) existiert einmal pro Prozess; GL-Objekte gehören
  zur Instanz und werden im ersten `Render()` angelegt.
- Zeitbasis: Analyse rechnet mit echter Zeit (Δt bis 1 s), Simulation mit Δt ≤ 0,1 s.

## 5. Audio-Analyse

Bildet den WebAudio-Analyser des Prototyps nach, damit das Verhalten übereinstimmt:
FFT 4096 mit Blackman-Fenster, Glättung 0,5 zwischen Bildern, Betrag → dB, Abbildung −100…−30 dB auf 0…1,
dann zurück in lineare Amplitude `10^((dB − (−30))/20)`.

### 5.1 Bänder

| Band | Bereich | Abklingzeit | Anschlag: Verhältnis zum Mittel | Sperrzeit |
|---|---|---|---|---|
| Bass | 25–120 Hz | 0,12 s | 1,45 | 0,22 s |
| Tiefen | 120–500 Hz | 0,25 s | 1,35 | 0,15 s |
| Mitten | 500–3000 Hz | 0,7 s | 1,30 | 0,30 s |
| Höhen | 3–16 kHz | 0,08 s | 1,50 | 0,07 s |

- Energie je Band: RMS der linearen Amplituden der Bins.
- **Pegel** relativ zum eigenen Langzeitmittel (τ = 8 s; liegt die Energie > 2,5× darüber,
  τ = 0,8 s): `norm = clamp(e / (1,7 · lang))`, unter 1e-4 → 0; `ziel = norm^1,3 · M^0,6`.
  Anstieg mit Faktor 0,6 pro Bild, Abfall mit der Abklingzeit des Bandes.
- **Anschlag**, wenn `e > Mittel · Verhältnis` und `e > 0,5 · lang` und `e` steigt und die
  Sperrzeit abgelaufen ist; Stärke `clamp((e/Mittel − 1) · 0,9) · M`. Mittel: τ 0,3 s
  (Mitten 0,6 s).
- **Tonhöhe** der Mitten: energiegewichteter Schwerpunkt auf logarithmischer Skala
  500 Hz → 0, 3 kHz → 1, geglättet (0,15 pro Bild).
- **Intensität**: mittlere Bandenergie, geglättet (τ 1,5 s), relativ zum Maximum der
  letzten ~40 s, quadriert, · M, geglättet (τ 0,8 s).
- **Drop**: Bass-Anschlag > 0,35 · M, Bass-Mittel der letzten ~3 s < 0,15 und Lautheit
  > 0,5 · Maximum; höchstens ein Drop pro 8 s.
- **Feinspektrum**: 32 logarithmische Bänder 30 Hz–14 kHz, gleiche Normierung (τ 8 s,
  Exponent 1,4); macht die Flamme unregelmäßig.
- Stille (Pause): alle Pegel fallen auf 0, nur der glimmende Ring bleibt.
- Nach Titelwechsel oder Sprung (Lücke > 1 s ohne Samples): Langzeitmittel neu einpegeln.

`M` = Musik-Intensität (Einstellung) / 100.

## 6. Szene „Eclipse“

Verhalten und Parameter entsprechen `docs/entwuerfe/dune-v14-favorit.html`; dort stehen
alle Zahlen. Die Tabelle fasst die Elemente zusammen. Logische Bezugsgröße 1280×720,
Ringradius `R = 0,19 · H · (1 + 0,05·atmen + 0,035·Wellengeburt + 0,012·sin(2πt/9))`.

| Element | Inhalt | Regler |
|---|---|---|
| Kern | weißer Kreis, schwarzes Loch innen (nichts darf hinein, auch kein Glow), feiner Begleitkreis | – |
| Flamme | 240 Zellen; Grundglut (Zufallsweg) + langsame Wellen + Feinspektrum + Puls-Buckel; Pixel steigen auf, werden zurückgezogen, ein Teil löst sich | Flamme |
| Freier Staub | losgelöste Pixel folgen dem gemeinsamen Strömungsfeld (Curl-Feld) und driften nach außen | Freie Partikel |
| Wellen | Ring löst sich vom Kern, anfangs über leuchtenden Schleier verbunden, läuft 7 s nach außen | Wellen |
| Corona | goldener Saum am Rand, 9 Streamer (gold → beige/eisblau, weit nach außen), Magnetbögen; weich gerendert (halbe Auflösung) + wenig scharfer Staub; kommt und geht | Corona |
| Glutfuß | Ring glüht golden, wo die Flamme hoch schlägt | Flamme |
| Bloom | verkleinerte Kopie additiv, Stärke 0,5 + 0,35 · Intensität | – |
| Drop | großer Flammenring, alle Streamer neu, Staubexplosion, Doppelwelle | Drop-Effekt |

Höchstzahl Staub 42 000 + Corona-Staub; bei Überlast wird gleichmäßig ausgedünnt.
Regler wirken sofort, auch auf bereits fliegende Teilchen (0 % = sofort weg).

### 6.1 Effekte und Zuordnung

| Effekt | Art | Wirkung | Standard-Band |
|---|---|---|---|
| Ring atmet | Pegel | Ringradius | Bass |
| Flammenhöhe | Pegel | Grundhöhe der Flamme (`0,18 + 1,2 · Pegel`) | Tiefen |
| Corona-Stärke | Pegel | hält Corona am Leben | Mitten |
| Staub löst sich | Pegel | Anteil losgelöster Pixel | Höhen |
| Strömung & Flimmern | Pegel | Tempo von Strömungsfeld und Flimmerwelle | Höhen |
| Welle | Anschlag | neue Welle, Ring schwillt an | Bass |
| Flammenstoß rundum | Anschlag | Buckelprofil rund um den Ring | Mitten |
| Stichflamme | Anschlag | 1–2 schmale Buckel | Tiefen |
| Streamer + Bogen | Anschlag | Streamer an der Stelle der Tonhöhe (tief unten, hoch oben, links/rechts zufällig), 45 % zusätzlich ein Bogen | Mitten |
| Staubausbruch | Anschlag | Staub reißt an 1–4 Stellen ab | Höhen |

Mehrere Bänder je Effekt: es zählt das stärkste. Kein Band: Effekt aus.

### 6.2 Pixelgröße

Stufen 0,5× · 0,75× · 1× · 1,5× · 2× · 3× · 4× · 6× · 8× (Standard 1×), bezogen auf
1280×720. Am Bildschirm: Blockkante `k = P · Bildhöhe / 720` Gerätepixel. `k ≥ 1`:
Quadrat mit Kante `round(k)`, eingerastet auf ein Raster dieser Größe. `k < 1`: 1 Gerätepixel
mit Alpha · k². Helligkeitsausgleich `P^−1,1` (P > 1) bzw. `P^−0,8` (P < 1).

## 7. Einstellungen

`resources/settings.xml`, Kategorien:

**Allgemein**

| ID | Typ | Bereich | Standard |
|---|---|---|---|
| `enabled` | bool | | an |
| `autoopen` | bool | | an |
| `scene` | Auswahl | `eclipse` | `eclipse` |
| `intensity` | Schieber | 1–100 | 70 |

**Eclipse**

| ID | Typ | Bereich | Standard |
|---|---|---|---|
| `eclipse.flame` | Schieber | 0–100 | 100 |
| `eclipse.waves` | Schieber | 0–100 | 100 |
| `eclipse.particles` | Schieber | 0–100 | 100 |
| `eclipse.corona` | Schieber | 0–100 | 100 |
| `eclipse.pixel` | Auswahl | 9 Stufen | 1× |
| `eclipse.drop` | bool | | an |
| `eclipse.route.<effekt>.<band>` | bool | 10 Effekte × 4 Bänder | wie §6.1 |

Effekt-Schlüssel: `breath, flame, corona, free, flow, wave, burst, jet, streamer, dust`;
Band-Schlüssel: `bass, low, mid, high`. In der Oberfläche ist die Zuordnung pro Effekt
als Gruppe mit vier Schaltern dargestellt.

**Leistung** (fortgeschritten): `maxparticles` 10 000–60 000, Standard 42 000.

Kodi bietet je Einstellung „Standard wiederherstellen“; zusätzlich gibt es einen Eintrag
„Alles auf Standard“ (ruft `action=reset`).

### 7.1 Live-Übernahme

Der Renderer liest beim Start die Kodi-Einstellungen. Änderungen (Einstellungsdialog
oder Bot) schreibt der Service zusätzlich nach `addon_data/visualization.spektra/live.json`
(atomar, mit steigender `revision`); der Renderer prüft die Datei höchstens einmal pro
Sekunde und übernimmt neue Werte ohne Neustart. Ungültige Werte werden auf den
gültigen Bereich begrenzt, unbekannte Schlüssel ignoriert.

## 8. Service und JSON-RPC

### 8.1 Verhalten bei Musik

Wie Party Video, aber unter der Bedingung: Spektra ist in `musicplayer.visualisation`
gewählt, `enabled` und `autoopen` sind an, `Player.HasAudio`, und weder
`spektra.suppress` noch `partyvideo.suppress` ist gesetzt.

- Titelstart: Visualisierung nach 0,25 / 0,75 / 1,5 s öffnen, Titelinfo ausblenden.
- Rückkehr nach 3 s Bedienpause aus Hauptmenü/Musikansichten, nie aus Einstellungen.
- Recovery: meldet `renderer.json` 5 s nach dem Öffnen keinen Herzschlag, Fenster
  höchstens zweimal neu öffnen.

Party Video und Spektra können gleichzeitig installiert sein; jeder Service handelt nur,
wenn sein Add-on gewählt ist.

### 8.2 Befehle

```json
{"jsonrpc":"2.0","id":1,"method":"Addons.ExecuteAddon",
 "params":{"addonid":"visualization.spektra","params":["action=set","key=intensity","value=40"]}}
```

| Aktion | Argumente | Wirkung |
|---|---|---|
| `on` | – | `enabled` an, Spektra als Visualisierung wählen, öffnen wenn Musik läuft |
| `off` | – | `enabled` aus, Fenster schließen, kein automatisches Öffnen |
| `toggle` | – | an/aus |
| `set` | `key`, `value` | eine Einstellung setzen; Schlüssel wie §7, Zuordnung zusätzlich kompakt: `key=route.wave value=bass+high` (leer = aus; Kodi trennt ExecuteAddon-Argumente am Komma) |
| `reset` | – | alle Einstellungen auf Standard |
| `scene` | `name` | Szene wählen (derzeit nur `eclipse`) |
| `status` | – | Status senden |
| `menu` | – (oder keine Argumente) | Menü: an/aus, Einstellungen, Status |

Fehler (unbekannte Aktion/Schlüssel, Wert außerhalb des Bereichs) kommen als
`state=error` mit `error=<code>`: `invalid_command`, `unknown_key`, `invalid_value`.
`ExecuteAddon` bestätigt nur die Übergabe; das Ergebnis kommt asynchron.

### 8.3 Status

`Other.spektra_status` über Kodis WebSocket und `Window(Home).Property(spektra.<feld>)`:

| Feld | Inhalt |
|---|---|
| `state` | `on`, `off` oder `error` |
| `visible` | Visualisierungsfenster offen (`true`/`false`) |
| `scene` | aktive Szene |
| `settings` | alle Einstellungen als Objekt (Property: JSON-Text) |
| `error` | Fehlercode oder leer |
| `revision` | steigt mit jeder Änderung |

`spektra.service=ready` bestätigt den laufenden Service. Doku dazu in `docs/bot-api.md`.

## 9. Fehlerverhalten

- GL-Initialisierung schlägt fehl: Fehler loggen, `renderer.json` meldet `error`, schwarzer
  Bildschirm statt Absturz.
- Keine Audiodaten: Pegel fallen auf 0, glimmender Ring.
- `live.json` fehlt/defekt: letzte gültige Werte behalten.
- Unbekannte Szene in den Einstellungen: Eclipse.

## 10. Tests

| Ebene | Was | Wo |
|---|---|---|
| C++ Unit (GoogleTest, Build-Container) | FFT gegen bekannte Sinus-Spektren; Analyzer: Kick-Folge → Bass-Anschläge im Takt, Sinus 1 kHz → nur Mitten, Rauschen 8 kHz → Höhen, Stille → Pegel 0, Sprung → Neueinpegeln, Drop nach Pause; Routing: Standard und Mehrfachzuordnung; Eclipse: nichts innerhalb von R, Regler 0 → keine Teilchen des Elements, Teilchenobergrenze, deterministisch mit festem Zufallssamen; Settings: Grenzen, Standardwerte, `live.json` | `tests/cpp` |
| Python Unit | Befehlsparser und -validierung, `set`-Schlüssel und Werte, Statusinhalt, Bedingung fürs automatische Öffnen (mit Fake-Adapter) | `tests/python` |
| Build | ELF für aarch64, Zip-Inhalt, ruff | `test.sh` |
| Pi (Maintainer installiert) | Sicht- und Verhaltensabgleich mit dem Prototyp, fps in `renderer.json`, Logs | manuell, nur lesend von hier |

## 11. Nicht in Version 1

Weitere Szenen, Testsong, Mikrofon-/Dateieingabe, Live-Bandtabelle und
Durchlauf-Funktion des Prototyps; Anpassungen am KodiMediaBot (eigener Schritt
nach Fertigstellung der Schnittstelle).
