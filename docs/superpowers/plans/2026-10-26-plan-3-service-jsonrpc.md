# Spektra Plan 3: Service, Menü und JSON-RPC – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Spektra verhält sich bei Musik wie Party Video (öffnet die Vollbild-Visualisierung selbst, kehrt nach Bedienpausen zurück, lässt sich unterdrücken, Recovery bei fehlendem Renderer) und ist per JSON-RPC (`Addons.ExecuteAddon`) steuerbar: an/aus, jede Einstellung setzen, Zuordnung kompakt setzen, alles zurücksetzen, Szene wählen, Status abfragen. Dazu ein kleines Menü und die Bot-Doku.

**Architecture:** Reine Python-Logik ohne Kodi (`resources/lib/spektra/`: `settings.py` Schema, `commands.py` Befehle, `controller.py` Zustandslogik, `statefile.py`) wird mit `unittest` geprüft. Nur `kodi.py`, `ui.py`, `default.py` und `service.py` importieren `xbmc*`. `default.py` nimmt `action=…`-Argumente entgegen und schickt sie per `JSONRPC.NotifyAll` an den Service; der Service schreibt Kodi-Einstellungen, `live.json` (vom Renderer übernommen) und den Status (`Other.spektra_status`, `Window(Home).Property(spektra.*)`).

**Tech Stack:** Python 3.11 (Kodi 21, `xbmc.python` 3.0.1), unittest, ruff; Basis Plan 1/2 (Renderer liest `live.json`, schreibt `renderer.json` mit `time`).

**Spec:** `docs/superpowers/specs/2026-10-26-spektra-design.md` (§3, §7, §7.1, §8, §9)

## Global Constraints

- Nur `kodi.py`, `ui.py`, `default.py`, `service.py` importieren `xbmc`, `xbmcaddon`, `xbmcgui`, `xbmcvfs`.
- ruff mit `target-version = "py311"`, Zeilenlänge 110, Regeln `E,F,W,I,UP,B` (wie Party Video).
- Schlüssel, Typen, Bereiche und Standardwerte exakt wie `src/core/Settings.cpp` und `resources/settings.xml` (Spec §7); Tests sichern das ab.
- `live.json`: `{"revision": <int>, "settings": {<schlüssel>: bool|int|str}}`, atomar geschrieben; `revision` = Millisekunden seit Epoche, streng steigend (überlebt Service-Neustarts, behebt Plan-1-Minor „kleinere Revision nach Neustart“).
- Automatisch öffnen nur, wenn: Spektra in `musicplayer.visualisation` gewählt, `enabled` und `autoopen` an, `Player.HasAudio`, weder `spektra.suppress` noch `partyvideo.suppress` gesetzt.
- Fehlercodes: `invalid_command`, `unknown_key`, `invalid_value` (Spec §8.2).
- **Nicht committen.** Jede Aufgabe endet mit „Stand prüfen“.
- Pi: Installation/Tests durch Claude freigegeben (2026-10-26); Musik nur 48 kHz; Testdateien danach löschen; Kodi-Einstellungen außer `musicplayer.visualisation` nicht ändern.

## Review Focus

1. Bot setzt eine Einstellung, während der Nutzer gleichzeitig im Einstellungsdialog ist → keine Endlosschleife zwischen `onSettingsChanged` und Service, letzter Stand gewinnt (Task 3: `ControllerTest.SyncDoesNotWriteSettings`; Task 5: `ServiceTest.SettingsChangeOnlySyncs`).
2. Service startet neu, während Kodi läuft → neue `live.json`-Revision liegt über der alten, Renderer übernimmt sie (Task 3: `ControllerTest.RevisionIsMonotonicAcrossRestarts`).
3. Ungültige Bot-Eingaben (fremde Aktion, unbekannter Schlüssel, Text statt Zahl, Wert außerhalb des Bereichs, unbekanntes Band) → Fehlerstatus, keine Einstellung verändert (Task 2/3: `test_rejects_*`, `ControllerTest.InvalidSetChangesNothing`).
4. Party Video ist zusätzlich installiert oder ein Telegram-Anruf setzt `partyvideo.suppress` → Spektra übernimmt das Fenster nicht (Task 5: `test_respects_both_suppress_flags`, `test_other_visualisation_is_left_alone`).
5. Renderer meldet keinen Herzschlag (GL-Problem, hängender Start) → höchstens zwei Wiederöffnungen, keine Navigationsschleife (Task 5: `RecoveryTest`).

## Dateistruktur

| Datei | Verantwortung |
|---|---|
| `pyproject.toml` | ruff-Konfiguration |
| `test.sh` | + ruff + Python-Tests |
| `visualization.spektra/addon.xml.in` | + Skript- und Service-Erweiterung, `xbmc.python` |
| `visualization.spektra/default.py` | Einstieg für Menü und JSON-RPC-Befehle |
| `visualization.spektra/service.py` | automatisches Öffnen, Rückkehr, Recovery, Befehlskanal |
| `visualization.spektra/resources/lib/spektra/__init__.py` | Paket |
| `…/spektra/settings.py` | Schema, Prüfen, Umwandeln, Kurzform `route.<effekt>` |
| `…/spektra/commands.py` | Befehle aus Argumenten/JSON prüfen |
| `…/spektra/statefile.py` | JSON lesen/atomar schreiben |
| `…/spektra/controller.py` | Zustandslogik: on/off/toggle/set/reset/scene/status, `live.json`, Status |
| `…/spektra/kodi.py` | Kodi-Adapter (Einstellungen, Fenster, Status, Befehlskanal) |
| `…/spektra/ui.py` | Menü, Fehlermeldungen |
| `visualization.spektra/resources/settings.xml` | + „Alles auf Standard“, + „Menü öffnen“ |
| `…/strings.po` (de, en) | neue Texte |
| `docs/bot-api.md` | Schnittstelle für den KodiMediaBot |
| `tests/python/test_settings.py`, `test_commands.py`, `test_controller.py`, `test_service.py` | Tests |

---

### Task 1: Python-Gerüst und Einstellungsschema

**Files:**
- Create: `pyproject.toml`, `visualization.spektra/resources/lib/spektra/__init__.py`, `visualization.spektra/resources/lib/spektra/settings.py`, `tests/python/test_settings.py`
- Modify: `test.sh`, `CLAUDE.md`

**Interfaces:**
- Produces:

```python
# spektra.settings
EFFECTS: tuple[str, ...]    # breath, flame, corona, free, flow, wave, burst, jet, streamer, dust
BANDS: tuple[str, ...]      # bass, low, mid, high
class SettingError(ValueError): code: str          # unknown_key | invalid_value
class Spec: kind ("bool"|"int"|"choice"), default, minimum, maximum, choices
SCHEMA: dict[str, Spec]
def parse(key: str, text: str) -> bool | int | str
def to_text(key: str, value) -> str
def expand(key: str, text: str) -> dict[str, bool | int | str]   # "route.<effekt>" = "bass,high" -> 4 Bool-Schlüssel
def defaults() -> dict
def read(getter) -> dict                                           # getter(key) -> str; Ungültiges -> Standard
```

- [ ] **Step 1: Gerüst**

`pyproject.toml`:

```toml
[tool.ruff]
target-version = "py311"
line-length = 110

[tool.ruff.lint]
select = ["E", "F", "W", "I", "UP", "B"]
```

```bash
python3 -m venv .venv && .venv/bin/pip install -q ruff
touch visualization.spektra/resources/lib/spektra/__init__.py
```

`test.sh` (ersetzt die Datei):

```bash
#!/usr/bin/env bash
# Alle Prüfungen: ruff (Python 3.11-Syntax), Python-Tests, Build-Skript-Tests und C++-Tests im
# Build-Container, danach das echte Zip.
set -euo pipefail
cd "$(dirname "$0")"

.venv/bin/ruff check .
python3 -B -m unittest discover -s tests/python -v
scripts/in-container.sh tests/build/test_check_elf.sh
scripts/in-container.sh tests/build/test_check_zip.sh
scripts/in-container.sh scripts/test-cpp-in-container.sh

# Auch Kodi-/GLES-Anbindung und das tatsächlich ausgelieferte Zip prüfen.
./build.sh
```

In `CLAUDE.md` unter „Regeln“ ergänzen:

```markdown
- Python läuft in Kodis Python 3.11; ruff prüft mit `target-version = "py311"`.
- Nur `kodi.py`, `ui.py`, `default.py` und `service.py` dürfen `xbmc*` importieren.
```

- [ ] **Step 2: Failing test**

`tests/python/test_settings.py`:

```python
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "visualization.spektra" / "resources" / "lib"))

from spektra import settings  # noqa: E402
from spektra.settings import SettingError  # noqa: E402


class SettingsTests(unittest.TestCase):
    def test_defaults_match_spec(self):
        d = settings.defaults()
        self.assertEqual(d["intensity"], 70)
        self.assertEqual(d["eclipse.pixel"], 2)
        self.assertEqual(d["maxparticles"], 42000)
        self.assertIs(d["eclipse.drop"], True)
        self.assertEqual(d["scene"], "eclipse")
        expected = {
            "breath": "bass", "flame": "low", "corona": "mid", "free": "high", "flow": "high",
            "wave": "bass", "burst": "mid", "jet": "low", "streamer": "mid", "dust": "high",
        }
        for effect in settings.EFFECTS:
            for band in settings.BANDS:
                self.assertIs(d[f"eclipse.route.{effect}.{band}"], expected[effect] == band, (effect, band))
        self.assertEqual(len(d), 10 + 40 + 1)

    def test_settings_xml_matches_schema(self):
        xml = (ROOT / "visualization.spektra" / "resources" / "settings.xml").read_text(encoding="utf-8")
        found = dict(re.findall(r'<setting id="([^"]+)"[^>]*default="([^"]+)"', xml))
        self.assertEqual(set(found), set(settings.SCHEMA))
        for key, text in found.items():
            self.assertEqual(settings.to_text(key, settings.defaults()[key]), text, key)

    def test_parse_accepts_kodi_and_bot_forms(self):
        self.assertEqual(settings.parse("intensity", "40"), 40)
        self.assertIs(settings.parse("eclipse.drop", "false"), False)
        self.assertIs(settings.parse("eclipse.drop", "1"), True)
        self.assertEqual(settings.parse("scene", "eclipse"), "eclipse")

    def test_rejects_bad_values(self):
        for key, text in (("intensity", "0"), ("intensity", "101"), ("intensity", "viel"),
                          ("intensity", "4.5"), ("eclipse.drop", "ja"), ("scene", "matrix"),
                          ("eclipse.pixel", "9")):
            with self.subTest(key=key, text=text):
                with self.assertRaises(SettingError) as ctx:
                    settings.parse(key, text)
                self.assertEqual(ctx.exception.code, "invalid_value")
        with self.assertRaises(SettingError) as ctx:
            settings.parse("farbe", "rot")
        self.assertEqual(ctx.exception.code, "unknown_key")

    def test_route_shorthand_expands_to_four_switches(self):
        self.assertEqual(
            settings.expand("route.wave", "bass,high"),
            {"eclipse.route.wave.bass": True, "eclipse.route.wave.low": False,
             "eclipse.route.wave.mid": False, "eclipse.route.wave.high": True},
        )
        self.assertEqual(set(settings.expand("route.dust", "").values()), {False})
        with self.assertRaises(SettingError) as ctx:
            settings.expand("route.wave", "bass,treble")
        self.assertEqual(ctx.exception.code, "invalid_value")
        with self.assertRaises(SettingError) as ctx:
            settings.expand("route.blitz", "bass")
        self.assertEqual(ctx.exception.code, "unknown_key")
        self.assertEqual(settings.expand("intensity", "55"), {"intensity": 55})

    def test_read_falls_back_to_defaults(self):
        stored = {"intensity": "40", "eclipse.flame": "kaputt"}
        values = settings.read(lambda key: stored.get(key, ""))
        self.assertEqual(values["intensity"], 40)
        self.assertEqual(values["eclipse.flame"], 100)
        self.assertIs(values["enabled"], True)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 3: Run test to verify it fails**

Run: `python3 -B -m unittest discover -s tests/python -v`
Expected: FAIL (`ModuleNotFoundError: No module named 'spektra.settings'`)

- [ ] **Step 4: Implementierung**

`visualization.spektra/resources/lib/spektra/settings.py`:

```python
"""Einstellungen von Spektra: Schlüssel, Typen, Bereiche und Standardwerte (wie src/core/Settings.cpp)."""

from dataclasses import dataclass

EFFECTS = ("breath", "flame", "corona", "free", "flow", "wave", "burst", "jet", "streamer", "dust")
BANDS = ("bass", "low", "mid", "high")
DEFAULT_ROUTE = {
    "breath": "bass", "flame": "low", "corona": "mid", "free": "high", "flow": "high",
    "wave": "bass", "burst": "mid", "jet": "low", "streamer": "mid", "dust": "high",
}
ROUTE_SHORTHAND = "route."


class SettingError(ValueError):
    def __init__(self, code):
        super().__init__(code)
        self.code = code


@dataclass(frozen=True)
class Spec:
    kind: str  # "bool" | "int" | "choice"
    default: object
    minimum: int = 0
    maximum: int = 0
    choices: tuple = ()


def _schema():
    schema = {
        "enabled": Spec("bool", True),
        "autoopen": Spec("bool", True),
        "scene": Spec("choice", "eclipse", choices=("eclipse",)),
        "intensity": Spec("int", 70, 1, 100),
    }
    for element in ("flame", "waves", "particles", "corona"):
        schema[f"eclipse.{element}"] = Spec("int", 100, 0, 100)
    schema["eclipse.pixel"] = Spec("int", 2, 0, 8)
    schema["eclipse.drop"] = Spec("bool", True)
    for effect in EFFECTS:
        for band in BANDS:
            schema[f"eclipse.route.{effect}.{band}"] = Spec("bool", DEFAULT_ROUTE[effect] == band)
    schema["maxparticles"] = Spec("int", 42000, 10000, 60000)
    return schema


SCHEMA = _schema()


def parse(key, text):
    spec = SCHEMA.get(key)
    if spec is None:
        raise SettingError("unknown_key")
    if not isinstance(text, str):
        raise SettingError("invalid_value")
    if spec.kind == "bool":
        if text in ("true", "1"):
            return True
        if text in ("false", "0"):
            return False
        raise SettingError("invalid_value")
    if spec.kind == "int":
        try:
            number = int(text, 10)
        except ValueError:
            raise SettingError("invalid_value") from None
        if not spec.minimum <= number <= spec.maximum:
            raise SettingError("invalid_value")
        return number
    if text not in spec.choices:
        raise SettingError("invalid_value")
    return text


def to_text(key, value):
    if SCHEMA[key].kind == "bool":
        return "true" if value else "false"
    return str(value)


def expand(key, text):
    """Ein Befehl kann mehrere Einstellungen betreffen: route.<effekt> = kommagetrennte Bänder."""
    if key.startswith(ROUTE_SHORTHAND):
        effect = key[len(ROUTE_SHORTHAND):]
        if effect not in EFFECTS:
            raise SettingError("unknown_key")
        bands = [band for band in text.split(",") if band] if isinstance(text, str) else None
        if bands is None or any(band not in BANDS for band in bands):
            raise SettingError("invalid_value")
        return {f"eclipse.route.{effect}.{band}": band in bands for band in BANDS}
    return {key: parse(key, text)}


def defaults():
    return {key: spec.default for key, spec in SCHEMA.items()}


def read(getter):
    """Alle Werte über getter(key) -> str lesen; Ungültiges oder Fehlendes wird zum Standardwert."""
    values = {}
    for key, spec in SCHEMA.items():
        try:
            values[key] = parse(key, getter(key))
        except SettingError:
            values[key] = spec.default
    return values
```

- [ ] **Step 5: Run test to verify it passes**

Run: `python3 -B -m unittest discover -s tests/python -v && .venv/bin/ruff check .`
Expected: 6 Tests OK; ruff `All checks passed!`

- [ ] **Step 6: Stand prüfen (nicht committen)**

---

### Task 2: Befehle und Zustandsdateien

**Files:**
- Create: `…/spektra/commands.py`, `…/spektra/statefile.py`, `tests/python/test_commands.py`

**Interfaces:**
- Produces:

```python
# spektra.commands
ACTIONS = ("on", "off", "toggle", "set", "reset", "scene", "status", "menu")
class CommandError(ValueError): code
@dataclass(frozen=True) class Command: action: str; key: str = ""; value: str | None = None; name: str = ""
    @classmethod from_data(data: dict) -> Command
    def to_data(self) -> dict
def parse_args(args: list[str]) -> Command          # leere Liste -> menu
# spektra.statefile
def read_json(path) -> dict                          # {} bei Fehler
def write_json(path, value)                          # atomar
```

- [ ] **Step 1: Failing test**

`tests/python/test_commands.py`:

```python
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "visualization.spektra" / "resources" / "lib"))

from spektra.commands import Command, CommandError, parse_args  # noqa: E402
from spektra.statefile import read_json, write_json  # noqa: E402


class CommandTests(unittest.TestCase):
    def test_parses_bot_arguments(self):
        self.assertEqual(parse_args([]), Command("menu"))
        self.assertEqual(parse_args(["action=on"]), Command("on"))
        self.assertEqual(
            parse_args(["action=set", "key=intensity", "value=40"]), Command("set", key="intensity", value="40")
        )
        self.assertEqual(parse_args(["action=set", "key=route.dust", "value="]),
                         Command("set", key="route.dust", value=""))
        self.assertEqual(parse_args(["action=scene", "name=eclipse"]), Command("scene", name="eclipse"))

    def test_round_trip_through_notification(self):
        for command in (Command("status"), Command("set", key="eclipse.drop", value="false"),
                        Command("set", key="route.dust", value=""), Command("scene", name="eclipse")):
            with self.subTest(command=command):
                self.assertEqual(Command.from_data(json.loads(json.dumps(command.to_data()))), command)

    def test_rejects_malformed_commands(self):
        for args in (["action=explode"], ["action=set", "key=intensity"], ["action=set", "value=1"],
                     ["action=on", "key=intensity"], ["action=scene"], ["action=status", "name=x"],
                     ["action"], ["action=on", "action=off"], ["action=on", "farbe=rot"]):
            with self.subTest(args=args):
                with self.assertRaises(CommandError) as ctx:
                    parse_args(args)
                self.assertEqual(ctx.exception.code, "invalid_command")
        for data in (None, [], {"action": 1}, {"action": "on", "extra": "x"}):
            with self.subTest(data=data):
                with self.assertRaises(CommandError):
                    Command.from_data(data)


class StatefileTests(unittest.TestCase):
    def test_write_is_atomic_and_read_is_forgiving(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sub" / "live.json"
            write_json(path, {"revision": 1})
            self.assertEqual(read_json(path), {"revision": 1})
            self.assertFalse(path.with_suffix(".json.tmp").exists())
            path.write_text("[1]")
            self.assertEqual(read_json(path), {})
            path.write_text("{")
            self.assertEqual(read_json(path), {})
            self.assertEqual(read_json(Path(tmp) / "fehlt.json"), {})


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -B -m unittest discover -s tests/python -v`
Expected: FAIL (`No module named 'spektra.commands'`)

- [ ] **Step 3: Implementierung**

`…/spektra/commands.py`:

```python
"""Befehle für Spektra: aus Addons.ExecuteAddon-Argumenten oder aus der Service-Benachrichtigung."""

from dataclasses import dataclass

ACTIONS = ("on", "off", "toggle", "set", "reset", "scene", "status", "menu")
FIELDS = ("action", "key", "value", "name")


class CommandError(ValueError):
    def __init__(self, code):
        super().__init__(code)
        self.code = code


@dataclass(frozen=True)
class Command:
    action: str
    key: str = ""
    value: str | None = None  # None = nicht angegeben; "" ist ein gültiger Wert (route.<effekt> = keine Bänder)
    name: str = ""

    @classmethod
    def from_data(cls, data):
        if not isinstance(data, dict) or set(data) - set(FIELDS):
            raise CommandError("invalid_command")
        if not all(isinstance(value, str) for value in data.values()):
            raise CommandError("invalid_command")
        action, key, name = data.get("action", ""), data.get("key", ""), data.get("name", "")
        value = data.get("value")
        if action not in ACTIONS:
            raise CommandError("invalid_command")
        if action == "set":
            if not key or value is None or name:
                raise CommandError("invalid_command")
        elif action == "scene":
            if not name or key or value is not None:
                raise CommandError("invalid_command")
        elif key or value is not None or name:
            raise CommandError("invalid_command")
        return cls(action, key, value, name)

    def to_data(self):
        data = {"action": self.action}
        if self.key:
            data["key"] = self.key
        if self.value is not None:
            data["value"] = self.value
        if self.name:
            data["name"] = self.name
        return data


def parse_args(args):
    data = {}
    for argument in args:
        key, separator, value = argument.partition("=")
        if not separator or key in data:
            raise CommandError("invalid_command")
        data[key] = value
    return Command.from_data(data or {"action": "menu"})
```

`…/spektra/statefile.py`:

```python
import json
import os
from pathlib import Path


def read_json(path):
    try:
        value = json.loads(Path(path).read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else {}
    except (OSError, ValueError):
        return {}


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    try:
        with temporary.open("w", encoding="utf-8") as stream:
            json.dump(value, stream, ensure_ascii=False)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -B -m unittest discover -s tests/python -v && .venv/bin/ruff check .`
Expected: 9 Tests OK; ruff ohne Befund

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 3: Controller

**Files:**
- Create: `…/spektra/controller.py`, `tests/python/test_controller.py`

**Interfaces:**
- Consumes: `settings` (Task 1), `Command`, `CommandError`, `write_json` (Task 2)
- Produces:

```python
class Controller:
    def __init__(self, adapter, live_path, clock=time.time)
    def start(self)                     # live.json + Status
    def command(self, command: Command)
    def sync(self)                      # Einstellungen geändert (Dialog): live.json + Status, schreibt keine Einstellungen
    def fail(self, code: str)
# adapter: get_setting(key)->str, set_setting(key, text), select(), has_audio()->bool,
#          visible()->bool, open_visualisation(), close_visualisation(), publish(status: dict)
```

Status (Spec §8.3): `{"state": "on"|"off"|"error", "visible": bool, "scene": str, "settings": dict, "error": str, "revision": int}`.

- [ ] **Step 1: Failing test**

`tests/python/test_controller.py`:

```python
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "visualization.spektra" / "resources" / "lib"))

from spektra import settings  # noqa: E402
from spektra.commands import Command  # noqa: E402
from spektra.controller import Controller  # noqa: E402
from spektra.statefile import read_json  # noqa: E402


class FakeAdapter:
    def __init__(self):
        self.stored = {key: settings.to_text(key, value) for key, value in settings.defaults().items()}
        self.selected = "visualization.partyvideo"
        self.audio = False
        self.window = False
        self.published = []
        self.writes = []

    def get_setting(self, key):
        return self.stored.get(key, "")

    def set_setting(self, key, text):
        self.writes.append((key, text))
        self.stored[key] = text

    def select(self):
        self.selected = "visualization.spektra"

    def has_audio(self):
        return self.audio

    def visible(self):
        return self.window and self.selected == "visualization.spektra"

    def open_visualisation(self):
        self.window = True

    def close_visualisation(self):
        self.window = False

    def publish(self, status):
        self.published.append(status)


class ControllerTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.live = Path(tmp.name) / "live.json"
        self.adapter = FakeAdapter()
        self.now = 1000.0
        self.controller = Controller(self.adapter, self.live, clock=lambda: self.now)
        self.controller.start()

    def last(self):
        return self.adapter.published[-1]

    def test_start_publishes_and_writes_live_json(self):
        self.assertEqual(self.last()["state"], "on")
        self.assertEqual(self.last()["scene"], "eclipse")
        self.assertEqual(read_json(self.live)["settings"]["intensity"], 70)
        self.assertEqual(read_json(self.live)["revision"], 1000000)

    def test_on_selects_spektra_and_opens_with_music(self):
        self.adapter.stored["enabled"] = "false"
        self.adapter.audio = True
        self.controller.command(Command("on"))
        self.assertEqual(self.adapter.selected, "visualization.spektra")
        self.assertTrue(self.adapter.window)
        self.assertEqual(self.adapter.stored["enabled"], "true")
        self.assertEqual(self.last()["state"], "on")
        self.assertTrue(self.last()["visible"])

    def test_off_closes_and_disables(self):
        self.adapter.select()
        self.adapter.window = True
        self.controller.command(Command("off"))
        self.assertFalse(self.adapter.window)
        self.assertEqual(self.adapter.stored["enabled"], "false")
        self.assertEqual(self.last()["state"], "off")

    def test_toggle_flips(self):
        self.controller.command(Command("toggle"))
        self.assertEqual(self.last()["state"], "off")
        self.controller.command(Command("toggle"))
        self.assertEqual(self.last()["state"], "on")

    def test_set_writes_setting_and_live_json(self):
        self.controller.command(Command("set", key="intensity", value="40"))
        self.assertEqual(self.adapter.stored["intensity"], "40")
        self.assertEqual(read_json(self.live)["settings"]["intensity"], 40)
        self.assertEqual(self.last()["settings"]["intensity"], 40)
        self.controller.command(Command("set", key="route.wave", value="mid"))
        self.assertEqual(self.adapter.stored["eclipse.route.wave.bass"], "false")
        self.assertEqual(self.adapter.stored["eclipse.route.wave.mid"], "true")

    def test_invalid_set_changes_nothing(self):
        for key, value, code in (("farbe", "rot", "unknown_key"), ("intensity", "viel", "invalid_value"),
                                 ("intensity", "500", "invalid_value"), ("route.wave", "treble", "invalid_value")):
            with self.subTest(key=key, value=value):
                self.adapter.writes.clear()
                self.controller.command(Command("set", key=key, value=value))
                self.assertEqual(self.adapter.writes, [])
                self.assertEqual(self.last()["state"], "error")
                self.assertEqual(self.last()["error"], code)
        self.controller.command(Command("status"))
        self.assertEqual(self.last()["state"], "on")
        self.assertEqual(self.last()["error"], "")

    def test_reset_restores_every_default(self):
        self.controller.command(Command("set", key="intensity", value="10"))
        self.controller.command(Command("set", key="route.burst", value="bass"))
        self.controller.command(Command("reset"))
        self.assertEqual(settings.read(self.adapter.get_setting), settings.defaults())

    def test_scene_accepts_only_known_scenes(self):
        self.controller.command(Command("scene", name="eclipse"))
        self.assertEqual(self.last()["state"], "on")
        self.controller.command(Command("scene", name="matrix"))
        self.assertEqual(self.last()["error"], "invalid_value")

    def test_menu_is_not_a_service_command(self):
        self.controller.command(Command("menu"))
        self.assertEqual(self.last()["error"], "invalid_command")

    def test_sync_does_not_write_settings(self):
        self.adapter.writes.clear()
        self.controller.sync()
        self.assertEqual(self.adapter.writes, [])

    def test_revision_is_monotonic_across_restarts(self):
        first = read_json(self.live)["revision"]
        self.controller.sync()
        self.assertGreater(read_json(self.live)["revision"], first)
        self.now = 999.0  # Uhr zurückgestellt
        restarted = Controller(self.adapter, self.live, clock=lambda: self.now)
        restarted.start()
        self.assertGreater(read_json(self.live)["revision"], first + 1)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -B -m unittest discover -s tests/python -v`
Expected: FAIL (`No module named 'spektra.controller'`)

- [ ] **Step 3: Implementierung**

`…/spektra/controller.py`:

```python
"""Zustandslogik des Service: Befehle ausführen, live.json schreiben, Status veröffentlichen."""

import time
from pathlib import Path

from . import settings
from .commands import CommandError
from .settings import SettingError
from .statefile import read_json, write_json


class Controller:
    def __init__(self, adapter, live_path, clock=time.time):
        self.adapter, self.live_path, self.clock = adapter, Path(live_path), clock
        # Revision steigt auch über Service-Neustarts und zurückgestellte Uhren hinweg.
        old = read_json(self.live_path).get("revision")
        self.revision = old if type(old) is int and old > 0 else 0
        self.error = ""

    def start(self):
        self.sync()

    def command(self, command):
        self.error = ""
        try:
            handler = getattr(self, "_" + command.action)
            handler(command)
        except (SettingError, CommandError) as exc:
            self.fail(exc.code)

    def sync(self):
        values = settings.read(self.adapter.get_setting)
        self.revision = max(self.revision + 1, int(self.clock() * 1000))
        write_json(self.live_path, {"revision": self.revision, "settings": values})
        self.publish(values)

    def fail(self, code):
        self.error = code
        self.publish()

    def publish(self, values=None):
        if values is None:
            values = settings.read(self.adapter.get_setting)
        state = "error" if self.error else ("on" if values["enabled"] else "off")
        self.adapter.publish({
            "state": state,
            "visible": bool(self.adapter.visible()),
            "scene": values["scene"],
            "settings": values,
            "error": self.error,
            "revision": self.revision,
        })

    def _apply(self, changes):
        for key, value in changes.items():
            self.adapter.set_setting(key, settings.to_text(key, value))
        self.sync()

    def _on(self, _command):
        self.adapter.set_setting("enabled", "true")
        self.adapter.select()
        if self.adapter.has_audio():
            self.adapter.open_visualisation()
        self.sync()

    def _off(self, _command):
        self.adapter.set_setting("enabled", "false")
        if self.adapter.visible():
            self.adapter.close_visualisation()
        self.sync()

    def _toggle(self, command):
        enabled = settings.read(self.adapter.get_setting)["enabled"]
        (self._off if enabled else self._on)(command)

    def _set(self, command):
        self._apply(settings.expand(command.key, command.value))

    def _reset(self, _command):
        self._apply(settings.defaults())

    def _scene(self, command):
        self._apply(settings.expand("scene", command.name))

    def _status(self, _command):
        self.publish()

    def _menu(self, _command):
        raise CommandError("invalid_command")  # das Menü läuft in default.py, nicht im Service
```

Hinweis: `test_start_publishes_and_writes_live_json` erwartet Revision `1000000` (= 1000 s · 1000); ohne alte Datei startet `self.revision` bei 0.

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -B -m unittest discover -s tests/python -v && .venv/bin/ruff check .`
Expected: 20 Tests OK; ruff ohne Befund

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 4: Kodi-Anbindung, Menü, Einstiegspunkt, Manifest

**Files:**
- Create: `…/spektra/kodi.py`, `…/spektra/ui.py`, `visualization.spektra/default.py`
- Modify: `visualization.spektra/addon.xml.in`, `visualization.spektra/resources/settings.xml`, beide `strings.po`

**Interfaces:**
- Consumes: Tasks 1–3
- Produces: `spektra.kodi`: `ADDON_ID`, `rpc(method, params)`, `paths() -> Path` (addon_data), `send(data: dict)`, `class Adapter` (Schnittstelle aus Task 3 plus `selected() -> str`); `spektra.ui`: `menu()`, `error(code)`.

Keine Unit-Tests (Kodi-Module); geprüft durch ruff, Build und auf dem Pi (Task 6).

- [ ] **Step 1: Kodi-Adapter**

`…/spektra/kodi.py`:

```python
import json
from pathlib import Path

import xbmc
import xbmcaddon
import xbmcgui
import xbmcvfs

ADDON_ID = "visualization.spektra"


def rpc(method, params):
    response = json.loads(
        xbmc.executeJSONRPC(json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params}))
    )
    if "error" in response:
        raise RuntimeError(response["error"].get("message", "Kodi RPC error"))
    return response["result"]


def paths():
    return Path(xbmcvfs.translatePath(xbmcaddon.Addon(ADDON_ID).getAddonInfo("profile")))


def send(data):
    if xbmcgui.Window(10000).getProperty("spektra.service") != "ready":
        raise RuntimeError("service_not_ready")
    rpc("JSONRPC.NotifyAll", {"sender": ADDON_ID, "message": "spektra_cmd", "data": data})


class Adapter:
    def get_setting(self, key):
        return xbmcaddon.Addon(ADDON_ID).getSetting(key)

    def set_setting(self, key, text):
        xbmcaddon.Addon(ADDON_ID).setSetting(key, text)

    def selected(self):
        return rpc("Settings.GetSettingValue", {"setting": "musicplayer.visualisation"})["value"]

    def select(self):
        if self.selected() != ADDON_ID and (
            rpc("Settings.SetSettingValue", {"setting": "musicplayer.visualisation", "value": ADDON_ID})
            is not True
        ):
            raise RuntimeError("Visualisierung konnte nicht umgeschaltet werden")

    def has_audio(self):
        return xbmc.getCondVisibility("Player.HasAudio")

    def visible(self):
        return xbmc.getCondVisibility("Window.IsActive(visualisation)") and self.selected() == ADDON_ID

    def open_visualisation(self):
        xbmc.executebuiltin("ActivateWindow(visualisation)")

    def close_visualisation(self):
        xbmc.executebuiltin("ActivateWindow(home)")

    def publish(self, status):
        window = xbmcgui.Window(10000)
        for key, value in status.items():
            if isinstance(value, bool):
                text = "true" if value else "false"
            elif isinstance(value, dict):
                text = json.dumps(value)
            else:
                text = str(value)
            window.setProperty("spektra." + key, text)
        rpc("JSONRPC.NotifyAll", {"sender": ADDON_ID, "message": "spektra_status", "data": status})
```

- [ ] **Step 2: Menü und Einstieg**

`…/spektra/ui.py`:

```python
import json

import xbmcaddon
import xbmcgui

from .kodi import ADDON_ID, send

ERRORS = ("invalid_command", "unknown_key", "invalid_value", "service_not_ready")


def text(number):
    return xbmcaddon.Addon(ADDON_ID).getLocalizedString(number)


def error(code):
    message = text(32090 + ERRORS.index(code)) if code in ERRORS else code
    xbmcgui.Dialog().notification("Spektra", message, xbmcgui.NOTIFICATION_ERROR, 5000)


def menu():
    dialog = xbmcgui.Dialog()
    enabled = xbmcaddon.Addon(ADDON_ID).getSetting("enabled") == "true"
    choice = dialog.select("Spektra", [text(32081 if enabled else 32080), text(32082), text(32083)])
    if choice == 0:
        send({"action": "off" if enabled else "on"})
    elif choice == 1:
        xbmcaddon.Addon(ADDON_ID).openSettings()
    elif choice == 2:
        send({"action": "status"})
        window = xbmcgui.Window(10000)
        lines = [f"{field}: {window.getProperty('spektra.' + field)}"
                 for field in ("state", "visible", "scene", "error", "revision")]
        settings = json.loads(window.getProperty("spektra.settings") or "{}")
        lines += [f"{key}: {value}" for key, value in settings.items() if not key.startswith("eclipse.route.")]
        dialog.textviewer("Spektra", "\n".join(lines))
```

`visualization.spektra/default.py`:

```python
"""Menü und Einstieg für JSON-RPC-Befehle (Addons.ExecuteAddon mit action=…)."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / "resources" / "lib"))
sys.dont_write_bytecode = True

from spektra import ui  # noqa: E402
from spektra.commands import CommandError, parse_args  # noqa: E402
from spektra.kodi import send  # noqa: E402


def main():
    try:
        command = parse_args(sys.argv[1:])
        if command.action == "menu":
            ui.menu()
        else:
            send(command.to_data())
    except CommandError as exc:
        ui.error(exc.code)
    except (OSError, ValueError, RuntimeError) as exc:
        ui.error(str(exc))


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Manifest, Einstellungen, Texte**

`visualization.spektra/addon.xml.in`: `<requires>@ADDON_DEPENDS@</requires>` ersetzen durch

```xml
  <requires>@ADDON_DEPENDS@
    <import addon="xbmc.python" version="3.0.1"/>
  </requires>
```

und nach der `xbmc.player.musicviz`-Zeile einfügen:

```xml
  <extension point="xbmc.python.script" library="default.py">
    <provides>executable</provides>
  </extension>
  <extension point="xbmc.service" library="service.py"/>
```

`visualization.spektra/resources/settings.xml`: in der ersten Kategorie nach `intensity` einfügen:

```xml
    <setting id="menu" type="action" label="32084" action="RunAddon(visualization.spektra,action=menu)" option="close" />
    <setting id="reset" type="action" label="32085" action="RunAddon(visualization.spektra,action=reset)" option="close" />
```

Beide `strings.po` ergänzen (deutsch `msgstr`, englisch `msgstr ""`):

| msgctxt | msgid | msgstr (de) |
|---|---|---|
| #32080 | Turn Spektra on | Spektra einschalten |
| #32081 | Turn Spektra off | Spektra ausschalten |
| #32082 | Settings | Einstellungen |
| #32083 | Status | Status |
| #32084 | Open menu | Menü öffnen |
| #32085 | Reset everything to default | Alles auf Standard |
| #32090 | Unknown command. | Unbekannter Befehl. |
| #32091 | Unknown setting. | Unbekannte Einstellung. |
| #32092 | Invalid value. | Ungültiger Wert. |
| #32093 | The Spektra service is not ready yet. Please wait. | Der Spektra-Service ist noch nicht bereit. Bitte kurz warten. |

- [ ] **Step 4: Prüfen**

Run: `.venv/bin/ruff check . && python3 -B -m unittest discover -s tests/python -v && scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: ruff ohne Befund; 20 Python-Tests OK (inkl. `test_settings_xml_matches_schema`, Aktionen haben kein `default`); C++ 75/75 (`SettingsXmlMatchesDefaults` zählt nur Einträge mit `default`)

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 5: Service

**Files:**
- Create: `visualization.spektra/service.py`, `tests/python/test_service.py`

**Interfaces:**
- Consumes: `Controller` (Task 3), `Adapter`, `paths`, `ADDON_ID` (Task 4), `Command`, `read_json`
- Produces: `service.takeover_allowed()`, `service.show_visualisation()`, `service.idle_return_due()`, `service.VisualisationRecovery(renderer_path, clock, wall)` mit `tick() -> bool` und `reset()`, `service.Monitor(commands, settings_changed)`, `service.Player`, `service.main()`.

- [ ] **Step 1: Failing test**

`tests/python/test_service.py`:

```python
import importlib.util
import json
import queue
import sys
import tempfile
import threading
import types
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

SERVICE = Path(__file__).resolve().parents[2] / "visualization.spektra" / "service.py"
SPEKTRA = "visualization.spektra"


class ServiceCase(unittest.TestCase):
    """Kodi-Attrappen und geladener Service; die Tests stehen in den Unterklassen."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.visible = {"Player.HasAudio", "Window.IsActive(visualisation)", "Player.ShowInfo"}
        self.settings = {"enabled": "true", "autoopen": "true"}
        self.selected = SPEKTRA
        self.kodi = types.SimpleNamespace(
            Monitor=object, Player=object, LOGINFO=1, LOGDEBUG=0, LOGWARNING=2, LOGERROR=3,
            log=Mock(),
            getGlobalIdleTime=Mock(return_value=3),
            getCondVisibility=lambda name: name in self.visible,
            executeJSONRPC=Mock(side_effect=lambda _r: json.dumps({"result": {"value": self.selected}})),
            executebuiltin=Mock(side_effect=self.builtin),
        )
        addon = Mock()
        addon.getSetting.side_effect = lambda key: self.settings.get(key, "")
        xbmcaddon = types.SimpleNamespace(Addon=Mock(return_value=addon))
        spec = importlib.util.spec_from_file_location("spektra_service", SERVICE)
        self.service = importlib.util.module_from_spec(spec)
        modules = {"xbmc": self.kodi, "xbmcaddon": xbmcaddon, "xbmcvfs": Mock(), "xbmcgui": Mock()}
        with patch.dict(sys.modules, modules), patch.object(sys, "dont_write_bytecode", True):
            spec.loader.exec_module(self.service)
        for name in [n for n in sys.modules if n == "spektra" or n.startswith("spektra.")]:
            del sys.modules[name]  # spektra.kodi wurde mit den Attrappen geladen

    def builtin(self, command):
        if command == "Action(Info)":
            self.visible.symmetric_difference_update({"Player.ShowInfo"})
        elif command == "ActivateWindow(visualisation)":
            self.visible.add("Window.IsActive(visualisation)")
            self.visible.discard("Window.IsActive(home)")
        elif command == "ActivateWindow(home)":
            self.visible.discard("Window.IsActive(visualisation)")
            self.visible.add("Window.IsActive(home)")
        elif command.startswith("Dialog.Close("):
            dialog = command.split("(")[1].split(",")[0]
            self.visible.discard(f"Window.IsActive({dialog})")


class ServiceTest(ServiceCase):
    # --- automatisches Öffnen ---
    def test_opens_visualisation_and_hides_info(self):
        self.visible = {"Player.HasAudio", "Window.IsActive(home)", "Player.ShowInfo"}
        for _ in range(3):
            self.service.show_visualisation()
        self.assertIn("Window.IsActive(visualisation)", self.visible)
        self.assertNotIn("Player.ShowInfo", self.visible)

    def test_info_is_hidden_without_toggling_back(self):
        self.service.show_visualisation()
        self.service.show_visualisation()
        self.kodi.executebuiltin.assert_called_once_with("Action(Info)")

    def test_respects_both_suppress_flags(self):
        for flag in self.service.SUPPRESSED:
            with self.subTest(flag=flag):
                self.visible = {"Player.HasAudio", "Window.IsActive(home)", flag}
                self.assertFalse(self.service.takeover_allowed())
                self.service.show_visualisation()
                self.kodi.executebuiltin.assert_not_called()

    def test_other_visualisation_is_left_alone(self):
        self.selected = "visualization.partyvideo"
        self.visible = {"Player.HasAudio", "Window.IsActive(home)"}
        self.service.show_visualisation()
        self.kodi.executebuiltin.assert_not_called()

    def test_disabled_or_no_autoopen_or_no_audio_does_nothing(self):
        for settings, visible in (({"enabled": "false", "autoopen": "true"}, {"Player.HasAudio"}),
                                  ({"enabled": "true", "autoopen": "false"}, {"Player.HasAudio"}),
                                  ({"enabled": "true", "autoopen": "true"}, set())):
            with self.subTest(settings=settings):
                self.settings, self.visible = settings, visible | {"Window.IsActive(home)"}
                self.service.show_visualisation()
                self.kodi.executebuiltin.assert_not_called()

    def test_dialogs_and_settings_stay_open(self):
        self.visible = {"Player.HasAudio", "Window.IsActive(home)", "System.HasModalDialog"}
        self.service.show_visualisation()
        self.kodi.executebuiltin.assert_not_called()
        self.visible = {"Player.HasAudio", "Window.IsActive(settings)"}
        self.assertFalse(self.service.idle_return_due())

    def test_idle_return_after_three_seconds(self):
        self.visible = {"Player.HasAudio", "Window.IsActive(home)"}
        self.kodi.getGlobalIdleTime.return_value = 2
        self.assertFalse(self.service.idle_return_due())
        self.kodi.getGlobalIdleTime.return_value = 3
        self.assertTrue(self.service.idle_return_due())

    # --- Befehlskanal ---
    def test_monitor_queues_own_commands_and_reports_settings_changes(self):
        commands, changed = queue.Queue(), threading.Event()
        monitor = self.service.Monitor(commands, changed)
        monitor.onNotification(SPEKTRA, "Other.spektra_cmd", json.dumps({"action": "status"}))
        monitor.onNotification("other.addon", "Other.spektra_cmd", json.dumps({"action": "off"}))
        monitor.onNotification(SPEKTRA, "Other.spektra_cmd", "{kaputt")
        self.assertEqual(commands.get_nowait(), {"action": "status"})
        self.assertTrue(commands.empty())
        monitor.onSettingsChanged()
        self.assertTrue(changed.is_set())

    def test_settings_change_only_syncs(self):
        controller = Mock()
        changed = threading.Event()
        changed.set()
        self.service.handle_settings_change(changed, controller)
        controller.sync.assert_called_once_with()
        self.assertFalse(changed.is_set())
        controller.command.assert_not_called()


class RecoveryTest(ServiceCase):
    def make(self, beat=None):
        path = Path(self.temp.name) / "renderer.json"
        if beat is not None:
            path.write_text(json.dumps({"state": "running", "time": beat}))
        self.now, self.wall = 0.0, 5000.0
        recovery = self.service.VisualisationRecovery(path, lambda: self.now, lambda: self.wall)
        self.assertFalse(recovery.tick())  # Fenster gerade gesehen
        return recovery, path

    def test_missing_heartbeat_reopens_at_most_twice(self):
        recovery, _ = self.make()
        self.now = 4.9
        self.assertFalse(recovery.tick())
        for attempt in (1, 2):
            self.now += 5.2
            self.assertTrue(recovery.tick())  # -> Hauptmenü
            self.assertTrue(recovery.tick())  # -> wieder Visualisierung
            self.assertIn("Window.IsActive(visualisation)", self.visible)
            self.assertFalse(recovery.tick())  # neues Öffnen beobachtet
        self.now += 10
        self.assertFalse(recovery.tick())
        self.assertEqual(self.kodi.executebuiltin.call_count, 4)

    def test_fresh_heartbeat_never_reopens(self):
        recovery, path = self.make(beat=5000)
        self.now = 6.0
        self.assertFalse(recovery.tick())
        self.kodi.executebuiltin.assert_not_called()

    def test_reset_allows_new_attempts(self):
        recovery, _ = self.make()
        recovery.attempts = 2
        recovery.reset()
        self.assertEqual(recovery.attempts, 0)

    def test_no_reopen_while_suppressed(self):
        recovery, _ = self.make()
        self.visible.add("!String.IsEmpty(Window(Home).Property(spektra.suppress))")
        self.now = 6.0
        self.assertFalse(recovery.tick())
        self.kodi.executebuiltin.assert_not_called()


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -B -m unittest discover -s tests/python -v`
Expected: FAIL (`service.py` fehlt: `FileNotFoundError`)

- [ ] **Step 3: Implementierung**

`visualization.spektra/service.py`:

```python
"""Öffnet Spektra beim Musikstart, kehrt nach Bedienpausen zurück und führt Befehle aus."""

import json
import queue
import sys
import threading
import time
from pathlib import Path

import xbmc
import xbmcaddon
import xbmcgui

sys.path.insert(0, str(Path(__file__).parent / "resources" / "lib"))
sys.dont_write_bytecode = True

from spektra.commands import Command, CommandError  # noqa: E402
from spektra.statefile import read_json  # noqa: E402

ADDON_ID = "visualization.spektra"
RETURN_AFTER_SECONDS = 3
# Andere Addons setzen diese Home-Fenster-Eigenschaften, solange ihr Ton keine Musik ist
# (z. B. script.telegram.calls während eines Anrufs); dann nicht auf die Visualisierung schalten.
SUPPRESSED = (
    "!String.IsEmpty(Window(Home).Property(spektra.suppress))",
    "!String.IsEmpty(Window(Home).Property(partyvideo.suppress))",
)


def selected_visualisation():
    try:
        response = json.loads(xbmc.executeJSONRPC(json.dumps({
            "jsonrpc": "2.0", "id": 1, "method": "Settings.GetSettingValue",
            "params": {"setting": "musicplayer.visualisation"},
        })))
        return response.get("result", {}).get("value", "")
    except (ValueError, AttributeError):
        return ""


def takeover_allowed():
    """Musik läuft, niemand unterdrückt, Spektra ist gewählt, eingeschaltet und darf automatisch öffnen."""
    if not xbmc.getCondVisibility("Player.HasAudio"):
        return False
    if any(xbmc.getCondVisibility(condition) for condition in SUPPRESSED):
        return False
    addon = xbmcaddon.Addon(ADDON_ID)
    if addon.getSetting("enabled") != "true" or addon.getSetting("autoopen") != "true":
        return False
    return selected_visualisation() == ADDON_ID


def idle_return_due():
    """Nach Bedienpause aus Hauptmenü/Musikansicht zurück, nicht aus Einstellungen."""
    return xbmc.getGlobalIdleTime() >= RETURN_AFTER_SECONDS and any(
        xbmc.getCondVisibility(f"Window.IsActive({window})")
        for window in ("home", "music", "musicplaylist", "visualisation", "musicosd", "songinformation")
    )


def show_visualisation():
    if not takeover_allowed():
        return
    music_dialogs = ("musicosd", "songinformation")
    if xbmc.getCondVisibility("System.HasModalDialog") and not any(
        xbmc.getCondVisibility(f"Window.IsActive({dialog})") for dialog in music_dialogs
    ):
        return
    for dialog in music_dialogs:
        if xbmc.getCondVisibility(f"Window.IsActive({dialog})"):
            xbmc.executebuiltin(f"Dialog.Close({dialog},true)")
            return  # GUI-Anweisungen asynchron; Zustand im nächsten Durchlauf neu lesen.
    if not xbmc.getCondVisibility("Window.IsActive(visualisation)"):
        xbmc.log(f"[{ADDON_ID}] öffne Visualisierung", xbmc.LOGINFO)
        xbmc.executebuiltin("ActivateWindow(visualisation)")
        return
    # Info ist ein Toggle: niemals senden, wenn die Information schon aus ist.
    if xbmc.getCondVisibility("Player.ShowInfo"):
        xbmc.executebuiltin("Action(Info)")


class VisualisationRecovery:
    """Fehlt 5 s nach dem Öffnen der Herzschlag des Renderers, Fenster höchstens zweimal neu öffnen."""

    def __init__(self, renderer_path, clock=time.monotonic, wall=time.time):
        self.path, self.clock, self.wall = Path(renderer_path), clock, wall
        self.opened = None  # (monotone Zeit, Uhrzeit) beim ersten Sehen des Fensters
        self.attempts = 0
        self.reopening_since = None

    def reset(self):
        self.attempts = 0

    def tick(self):
        """True während eines Fensterwechsels: andere automatische Navigation aussetzen."""
        now = self.clock()
        if self.reopening_since is not None:
            if xbmc.getCondVisibility("Window.IsActive(home)"):
                xbmc.executebuiltin("ActivateWindow(visualisation)")
                self.reopening_since = None
                self.opened = None
                return True
            if now - self.reopening_since > 2.0:
                self.reopening_since = None  # Kodi hat nicht gewechselt: keine Navigationsschleife
            return self.reopening_since is not None
        if not xbmc.getCondVisibility("Window.IsActive(visualisation)"):
            self.opened = None
            return False
        if self.opened is None:
            self.opened = (now, self.wall())
            return False
        if now - self.opened[0] < 5.0 or self.attempts >= 2 or not takeover_allowed():
            return False
        beat = read_json(self.path).get("time")
        if isinstance(beat, (int, float)) and beat >= self.opened[1] - 1:
            return False
        self.attempts += 1
        self.reopening_since = now
        xbmc.log(f"[{ADDON_ID}] Renderer ohne Herzschlag, öffne neu (Versuch {self.attempts}/2)",
                 xbmc.LOGWARNING)
        xbmc.executebuiltin("ActivateWindow(home)")
        return True


class Player(xbmc.Player):
    def __init__(self):
        super().__init__()
        self.track_started = threading.Event()
        self.track_started.set()  # Auch beim Service-Neustart während laufender Musik.

    def onAVStarted(self):
        self.track_started.set()


class Monitor(xbmc.Monitor):
    def __init__(self, commands, settings_changed):
        super().__init__()
        self.commands, self.settings_changed = commands, settings_changed

    def onNotification(self, sender, method, data):
        if sender == ADDON_ID and method == "Other.spektra_cmd":
            try:
                self.commands.put_nowait(json.loads(data))
            except (ValueError, queue.Full):
                xbmc.log(f"[{ADDON_ID}] ungültiger Befehl oder Warteschlange voll", xbmc.LOGWARNING)

    def onSettingsChanged(self):
        self.settings_changed.set()


def handle_settings_change(changed, controller):
    """Einstellungsdialog geändert: live.json und Status nachziehen, selbst nichts schreiben."""
    if changed.is_set():
        changed.clear()
        controller.sync()


def main():
    from spektra.controller import Controller
    from spektra.kodi import Adapter, paths

    profile = paths()
    commands = queue.Queue(maxsize=64)
    changed = threading.Event()
    monitor = Monitor(commands, changed)
    player = Player()
    controller = Controller(Adapter(), profile / "live.json")
    recovery = VisualisationRecovery(profile / "renderer.json")
    window = xbmcgui.Window(10000)
    window.clearProperty("spektra.service")
    try:
        controller.start()
        window.setProperty("spektra.service", "ready")
        xbmc.log(f"[{ADDON_ID}] service: ready", xbmc.LOGINFO)
        pending = []
        next_idle_check = 0.0
        while not monitor.abortRequested():
            try:
                for _ in range(16):
                    try:
                        data = commands.get_nowait()
                    except queue.Empty:
                        break
                    try:
                        controller.command(Command.from_data(data))
                    except CommandError as exc:
                        controller.fail(exc.code)
                handle_settings_change(changed, controller)
                recovering = recovery.tick()
                if player.track_started.is_set():
                    player.track_started.clear()
                    recovery.reset()
                    now = time.monotonic()
                    pending = [now + delay for delay in (0.25, 0.75, 1.5)]
                if not recovering and pending and time.monotonic() >= pending[0]:
                    pending.pop(0)
                    show_visualisation()
                now = time.monotonic()
                if not recovering and not pending and now >= next_idle_check:
                    next_idle_check = now + 0.5
                    if idle_return_due():
                        show_visualisation()
            except Exception as exc:  # noqa: BLE001 – Service darf nicht sterben
                xbmc.log(f"[{ADDON_ID}] Service-Fehler: {exc}", xbmc.LOGERROR)
            if monitor.waitForAbort(0.1):
                break
    finally:
        window.clearProperty("spektra.service")
        xbmc.log(f"[{ADDON_ID}] service: beendet", xbmc.LOGINFO)


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -B -m unittest discover -s tests/python -v && .venv/bin/ruff check .`
Expected: 33 Tests OK; ruff ohne Befund

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 6: Bot-Doku, Gesamtprüfung, Pi

**Files:**
- Create: `docs/bot-api.md`

- [ ] **Step 1: Bot-Doku**

`docs/bot-api.md`:

````markdown
# Spektra per JSON-RPC steuern

Kodis vorhandener JSON-RPC-Zugang und dessen Zugangsdaten werden benutzt. `params` ist stets ein
Array von `key=value`-Strings.

```json
{"jsonrpc":"2.0","id":1,"method":"Addons.ExecuteAddon","params":{"addonid":"visualization.spektra","params":["action=set","key=intensity","value=40"]}}
```

| Aktion | Argumente | Wirkung |
|---|---|---|
| Einschalten | `["action=on"]` | Spektra als Kodi-Visualisierung wählen, öffnen, wenn Musik läuft |
| Ausschalten | `["action=off"]` | Fenster schließen, kein automatisches Öffnen |
| Umschalten | `["action=toggle"]` | an/aus |
| Einstellung setzen | `["action=set","key=<schlüssel>","value=<wert>"]` | eine Einstellung live ändern |
| Zuordnung setzen | `["action=set","key=route.<effekt>","value=bass,high"]` | Bänder eines Effekts; `value=` leer = Effekt aus |
| Zurücksetzen | `["action=reset"]` | alle Einstellungen auf Standard |
| Szene | `["action=scene","name=eclipse"]` | Szene wählen (derzeit nur `eclipse`) |
| Status | `["action=status"]` | Status senden |
| Menü | `["action=menu"]` oder keine Argumente | Menü am Fernseher öffnen |

## Schlüssel

| Schlüssel | Werte | Standard |
|---|---|---|
| `enabled`, `autoopen` | `true`/`false` | `true` |
| `scene` | `eclipse` | `eclipse` |
| `intensity` | 1–100 | 70 |
| `eclipse.flame`, `eclipse.waves`, `eclipse.particles`, `eclipse.corona` | 0–100 | 100 |
| `eclipse.pixel` | 0–8 = 0,5× · 0,75× · 1× · 1,5× · 2× · 3× · 4× · 6× · 8× | 2 (1×) |
| `eclipse.drop` | `true`/`false` | `true` |
| `eclipse.route.<effekt>.<band>` | `true`/`false` | siehe unten |
| `maxparticles` | 10000–60000 | 42000 |

Effekte: `breath` (Ring atmet), `flame` (Flammenhöhe), `corona` (Corona-Stärke), `free` (Staub löst
sich), `flow` (Strömung & Flimmern), `wave` (Welle), `burst` (Flammenstoß rundum), `jet`
(Stichflamme), `streamer` (Streamer + Bogen), `dust` (Staubausbruch). Bänder: `bass`, `low`, `mid`,
`high`. Standard: breath→bass, flame→low, corona→mid, free→high, flow→high, wave→bass, burst→mid,
jet→low, streamer→mid, dust→high.

## Status

Erfolg und Fehler kommen asynchron als `Other.spektra_status` über Kodis WebSocket;
`ExecuteAddon` bestätigt nur die Übergabe. Das interne `Other.spektra_cmd` kann der Bot ignorieren.

| Feld | Inhalt |
|---|---|
| `state` | `on`, `off` oder `error` |
| `visible` | Visualisierungsfenster offen |
| `scene` | aktive Szene |
| `settings` | alle Einstellungen |
| `error` | `invalid_command`, `unknown_key`, `invalid_value` oder leer |
| `revision` | steigt mit jeder Änderung |

Abfrage auch per `XBMC.GetInfoLabels` auf `Window(Home).Property(spektra.<feld>)` (`settings` als
JSON-Text). `spektra.service=ready` bestätigt den laufenden Service.

## Automatisches Öffnen unterdrücken

Spektra öffnet die Visualisierung bei Musikstart nur, wenn es in Kodi als Visualisierung gewählt
ist. Spielt ein anderes Addon Ton ab, der keine Musik ist, setzt es solange
`Window(10000).setProperty("spektra.suppress", "<addon-id>")` und löscht sie danach.
`partyvideo.suppress` wird ebenfalls beachtet (Telegram-Anrufe).
````

- [ ] **Step 2: Gesamtprüfung**

Run: `./test.sh`
Expected: ruff ohne Befund, 33 Python-Tests OK, Prüfskripte 6/6 und 7/7, C++ 75/75, Build Exit 0; Zip enthält `default.py`, `service.py`, `resources/lib/spektra/*.py` und kein `__pycache__`.

- [ ] **Step 3: Auf dem Pi installieren und Service starten**

Nur Python und Manifest ändern sich; die `.so` ist unverändert. Statt Kodi-Neustart das Add-on aus- und einschalten, damit Kodi Skript und Service neu einliest:

```bash
scp -q dist/visualization.spektra-0.1.0.zip root@192.168.178.10:/storage/downloads/
ssh root@192.168.178.10 'cd /storage/.kodi/addons && rm -rf visualization.spektra && unzip -q /storage/downloads/visualization.spektra-0.1.0.zip && rm /storage/downloads/visualization.spektra-0.1.0.zip && kodi-send --action="UpdateLocalAddons"'
```

dann per JSON-RPC `Addons.SetAddonEnabled` `false`, 2 s warten, `true`, 5 s warten. Prüfen: `XBMC.GetInfoLabels` → `Window(Home).Property(spektra.service)` = `ready`.

- [ ] **Step 4: Befehle über JSON-RPC prüfen**

Per `curl` auf `127.0.0.1:8080/jsonrpc` (auf dem Pi) nacheinander `Addons.ExecuteAddon` mit:
1. `action=status` → `spektra.state` = `on`, `spektra.revision` > 0.
2. `action=set, key=intensity, value=40` → `live.json` enthält `"intensity": 40`; `Addons.GetAddonDetails` nicht nötig.
3. `action=set, key=route.wave, value=bass,high` → `live.json` `eclipse.route.wave.high` = `true`.
4. `action=set, key=farbe, value=rot` → `spektra.state` = `error`, `spektra.error` = `unknown_key`.
5. `action=reset` → `live.json` `intensity` = 70, `eclipse.route.wave.high` = `false`.

- [ ] **Step 5: Automatisches Öffnen prüfen**

48-kHz-Testdatei kopieren, im Hauptmenü `Player.Open`; nach 3 s `GUI.GetProperties currentwindow` = Audiovisualisierung. `ActivateWindow(home)`, 5 s warten (Bedienpause) → wieder Visualisierung. Danach `action=off` → Fenster geschlossen, `spektra.state` = `off`; `Player.Open` erneut → bleibt im Hauptmenü. `action=on` → Visualisierung öffnet. Wiedergabe stoppen, Testdatei löschen. Am Ende ist Spektra eingeschaltet und gewählt.

Ergebnisse ins Ledger.

- [ ] **Step 6: Stand prüfen (nicht committen)**
