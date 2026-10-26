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
