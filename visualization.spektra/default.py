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
        elif command.action == "calibrate":
            ui.calibrate()
        else:
            send(command.to_data())
    except CommandError as exc:
        # Auch der Bot soll den Fehler im Status sehen, nicht nur der Fernseher.
        try:
            send({"error": exc.code})
        except RuntimeError:
            ui.error(exc.code)
    except (OSError, ValueError, RuntimeError) as exc:
        ui.error(str(exc))


if __name__ == "__main__":
    main()
