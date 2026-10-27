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
    schema["eclipse.size"] = Spec("int", 100, 50, 150)
    schema["eclipse.pulse"] = Spec("int", 100, 0, 300)
    schema["eclipse.palette"] = Spec("int", 0, 0, 3)
    schema["eclipse.flowstrength"] = Spec("int", 100, 0, 200)
    schema["eclipse.flowspeed"] = Spec("int", 100, 0, 200)
    schema["eclipse.reach"] = Spec("int", 100, 25, 200)
    for effect in EFFECTS:
        for band in BANDS:
            schema[f"eclipse.route.{effect}.{band}"] = Spec("bool", DEFAULT_ROUTE[effect] == band)
    for band in BANDS:
        schema[f"sensitivity.{band}"] = Spec("int", 100, 0, 200)
    schema["response"] = Spec("int", 100, 25, 400)
    schema["onset"] = Spec("int", 100, 50, 200)
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
    """Ein Befehl kann mehrere Einstellungen betreffen: route.<effekt> = Bänder, getrennt durch + oder ,

    Addons.ExecuteAddon trennt Argumente an Kommas; über JSON-RPC deshalb + verwenden.
    """
    if key.startswith(ROUTE_SHORTHAND):
        effect = key[len(ROUTE_SHORTHAND):]
        if effect not in EFFECTS:
            raise SettingError("unknown_key")
        if not isinstance(text, str):
            raise SettingError("invalid_value")
        bands = [band for band in text.replace("+", ",").split(",") if band]
        if any(band not in BANDS for band in bands):
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
