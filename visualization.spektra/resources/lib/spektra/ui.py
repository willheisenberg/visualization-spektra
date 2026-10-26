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
        lines += [f"{key}: {value}" for key, value in settings.items()
                  if not key.startswith("eclipse.route.")]
        dialog.textviewer("Spektra", "\n".join(lines))
