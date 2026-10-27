"""Kalibrieren über der laufenden Visualisierung: welche Regler, in welchen Schritten, wie angezeigt."""

from . import settings

# (Schlüssel, Beschriftung aus strings.po, Schritt je Tastendruck), in der Reihenfolge der Liste
ITEMS = (
    ("eclipse.size", 32017, 5),
    ("eclipse.pulse", 32018, 10),
    ("eclipse.palette", 32019, 1),
    ("eclipse.flame", 32011, 5),
    ("eclipse.waves", 32012, 5),
    ("eclipse.particles", 32013, 5),
    ("eclipse.corona", 32014, 5),
    ("eclipse.flowstrength", 32041, 10),
    ("eclipse.flowspeed", 32042, 10),
    ("eclipse.reach", 32043, 5),
    ("eclipse.pixel", 32015, 1),
    ("intensity", 32004, 5),
    ("sensitivity.bass", 32071, 5),
    ("sensitivity.low", 32072, 5),
    ("sensitivity.mid", 32073, 5),
    ("sensitivity.high", 32074, 5),
    ("response", 32075, 5),
    ("onset", 32076, 5),
)

# Auswahlwerte: Beschriftung je Wert statt einer Zahl
CHOICES = {
    "eclipse.palette": (32100, 32101, 32102, 32103),
    "eclipse.pixel": tuple(range(32020, 32029)),
}

_STEPS = {key: step for key, _label, step in ITEMS}


def stepped(key, value, direction):
    """Wert nach einem Tastendruck nach links (-1) oder rechts (+1), innerhalb des Bereichs."""
    spec = settings.SCHEMA[key]
    return max(spec.minimum, min(spec.maximum, value + direction * _STEPS[key]))


def display(key, value, text):
    """Anzeige eines Werts; text(nummer) liefert übersetzte Beschriftungen."""
    if key in CHOICES:
        return text(CHOICES[key][value])
    return str(value)
