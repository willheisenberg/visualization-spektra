# CLAUDE.md

## Befehle

    ./build.sh    # Addon-Zip bauen (Docker) → dist/
    ./test.sh     # Prüfskripte, C++-Tests, Build

## Regeln

- Nie committen oder pushen ohne ausdrückliche Anweisung.
- Pi `root@192.168.178.10`: nur lesender Zugriff (Logs, addon_data). Kein scp, keine
  Installation, kein Neustart – das macht der Maintainer.
- Die Addon-Version steht nur in `visualization.spektra/addon.xml.in`.
- Kerneinheiten in `src/core/` und Szenen in `src/scene/` kennen weder Kodi noch GL.
- Python läuft in Kodis Python 3.11; ruff prüft mit `target-version = "py311"`.
- Nur `kodi.py`, `ui.py`, `default.py` und `service.py` dürfen `xbmc*` importieren.
- Referenz für Aussehen und Parameter: `docs/entwuerfe/dune-v14-favorit.html`.
- Knisternder, zu hoher Ton bei 44,1-kHz-Musik kommt vom HDMI-Weg zum Denon, nicht von
  Spektra (A/B-Test 2026-10-26: 44,1 kHz ohne Visualisierung knistert, 48 kHz mit Spektra
  sauber; siehe auch `~/githubprojects/kodi-telegram-calls/CLAUDE.md`). Tests auf dem Pi mit
  48-kHz-Dateien machen. Kodi-Audioeinstellungen nur nach Rückfrage ändern.
