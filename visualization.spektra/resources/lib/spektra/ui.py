import json
import threading

import xbmc
import xbmcaddon
import xbmcgui

from . import calibration, settings
from .kodi import ADDON_ID, send

ERRORS = ("invalid_command", "unknown_key", "invalid_value", "service_not_ready")
ACTION_LEFT, ACTION_RIGHT = 1, 2
ACTION_CLOSE = (10, 92)  # Menü zurück, Zurück
SHOW_MENU_AFTER = 1.5  # Sekunden nach dem letzten Links/Rechts


def text(number):
    return xbmcaddon.Addon(ADDON_ID).getLocalizedString(number)


def error(code):
    message = text(32090 + ERRORS.index(code)) if code in ERRORS else code
    xbmcgui.Dialog().notification("Spektra", message, xbmcgui.NOTIFICATION_ERROR, 5000)


def menu():
    dialog = xbmcgui.Dialog()
    enabled = xbmcaddon.Addon(ADDON_ID).getSetting("enabled") == "true"
    entries = [text(32081 if enabled else 32080), text(32082), text(32086), text(32083)]
    choice = dialog.select("Spektra", entries)
    if choice == 0:
        send({"action": "off" if enabled else "on"})
    elif choice == 1:
        xbmcaddon.Addon(ADDON_ID).openSettings()
    elif choice == 2:
        calibrate()
    elif choice == 3:
        send({"action": "status"})
        window = xbmcgui.Window(10000)
        lines = [f"{field}: {window.getProperty('spektra.' + field)}"
                 for field in ("state", "visible", "scene", "error", "revision")]
        current = json.loads(window.getProperty("spektra.settings") or "{}")
        lines += [f"{key}: {value}" for key, value in current.items()
                  if not key.startswith("eclipse.route.")]
        dialog.textviewer("Spektra", "\n".join(lines))


def calibrate():
    """Kalibrierfenster über der laufenden Visualisierung; ohne Musik gibt es nichts zu sehen."""
    if not xbmc.getCondVisibility("Player.HasAudio"):
        xbmcgui.Dialog().notification("Spektra", text(32095), xbmcgui.NOTIFICATION_INFO, 5000)
        return
    send({"action": "on"})
    monitor = xbmc.Monitor()
    for _ in range(30):
        if xbmc.getCondVisibility("Window.IsActive(visualisation)"):
            break
        if monitor.waitForAbort(0.1):
            return
    path = xbmcaddon.Addon(ADDON_ID).getAddonInfo("path")
    dialog = CalibrationDialog("spektra-calibrate.xml", path, "default", "1080i")
    dialog.doModal()
    dialog.stop()
    del dialog


class CalibrationDialog(xbmcgui.WindowXMLDialog):
    """Liste aller Regler; bei Links/Rechts bleibt nur die Zeile des Reglers über dem Bild stehen."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.values = settings.read(xbmcaddon.Addon(ADDON_ID).getSetting)
        self.timer = None
        self.lock = threading.Lock()
        self.listing = None

    def onInit(self):
        self.listing = self.getControl(100)
        self.listing.reset()
        for key, label, _step in calibration.ITEMS:
            item = xbmcgui.ListItem(text(label))
            item.setLabel2(calibration.display(key, self.values[key], text))
            self.listing.addItem(item)
        self.setFocusId(100)

    def onAction(self, action):
        action_id = action.getId()
        if action_id in ACTION_CLOSE:
            self.close()
        elif action_id in (ACTION_LEFT, ACTION_RIGHT):
            self.adjust(-1 if action_id == ACTION_LEFT else 1)

    def adjust(self, direction):
        position = self.listing.getSelectedPosition() if self.listing else -1
        if position < 0:
            return
        key, label, _step = calibration.ITEMS[position]
        value = calibration.stepped(key, self.values[key], direction)
        shown = calibration.display(key, value, text)
        if value != self.values[key]:
            self.values[key] = value
            self.listing.getListItem(position).setLabel2(shown)
            send({"action": "set", "key": key, "value": str(value)})
        self.setProperty("bar.label", text(label))
        self.setProperty("bar.value", shown)
        self.setProperty("adjusting", "true")
        with self.lock:
            if self.timer:
                self.timer.cancel()
            self.timer = threading.Timer(SHOW_MENU_AFTER, self.clearProperty, ("adjusting",))
            self.timer.start()

    def stop(self):
        with self.lock:
            if self.timer:
                self.timer.cancel()
