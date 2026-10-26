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
from spektra.controller import ERRORS  # noqa: E402
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
        # Herzschlag seit dem Öffnen gesehen: bis zum nächsten Öffnen nichts mehr prüfen.
        self.confirmed = False
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
            self.confirmed = False
            return False
        if self.confirmed or now - self.opened[0] < 5.0 or self.attempts >= 2:
            return False
        beat = read_json(self.path).get("time")
        if isinstance(beat, (int, float)) and beat >= self.opened[1] - 1:
            self.confirmed = True
            return False
        if not takeover_allowed():
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


def handle_message(data, controller):
    """Befehl aus default.py ausführen; {"error": code} meldet einen dort verworfenen Befehl."""
    if isinstance(data, dict) and set(data) == {"error"}:
        controller.fail(data["error"] if data["error"] in ERRORS else "invalid_command")
        return
    try:
        controller.command(Command.from_data(data))
    except CommandError as exc:
        controller.fail(exc.code)


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
                    handle_message(data, controller)
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
