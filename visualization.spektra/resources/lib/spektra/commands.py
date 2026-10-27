"""Befehle für Spektra: aus Addons.ExecuteAddon-Argumenten oder aus der Service-Benachrichtigung."""

from dataclasses import dataclass

ACTIONS = ("on", "off", "toggle", "set", "reset", "scene", "status", "menu", "calibrate")
FIELDS = ("action", "key", "value", "name")


class CommandError(ValueError):
    def __init__(self, code):
        super().__init__(code)
        self.code = code


@dataclass(frozen=True)
class Command:
    action: str
    key: str = ""
    # None = nicht angegeben; "" ist ein gültiger Wert (route.<effekt> = keine Bänder)
    value: str | None = None
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
