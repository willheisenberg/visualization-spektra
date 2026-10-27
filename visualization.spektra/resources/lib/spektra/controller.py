"""Zustandslogik des Service: Befehle ausführen, live.json schreiben, Status veröffentlichen."""

import time
from pathlib import Path

from . import settings
from .commands import CommandError
from .settings import SettingError
from .statefile import read_json, write_json

ERRORS = ("invalid_command", "unknown_key", "invalid_value")


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

    def sync(self, changes=None):
        # Eben gesetzte Werte zählen, auch wenn Kodi sie noch nicht gespeichert hat
        # (bei offenem Einstellungsdialog speichert Kodi erst beim Schließen).
        values = {**settings.read(self.adapter.get_setting), **(changes or {})}
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
        self.sync(changes)

    def _on(self, _command):
        self.adapter.set_setting("enabled", "true")
        self.adapter.select()
        if self.adapter.has_audio():
            self.adapter.open_visualisation()
        self.sync({"enabled": True})

    def _off(self, _command):
        self.adapter.set_setting("enabled", "false")
        if self.adapter.visible():
            self.adapter.close_visualisation()
        self.sync({"enabled": False})

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

    def _calibrate(self, _command):
        raise CommandError("invalid_command")  # das Kalibrierfenster ebenso
