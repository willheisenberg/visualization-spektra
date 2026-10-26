import importlib.util
import json
import queue
import sys
import tempfile
import threading
import types
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

SERVICE = Path(__file__).resolve().parents[2] / "visualization.spektra" / "service.py"
SPEKTRA = "visualization.spektra"


class ServiceCase(unittest.TestCase):
    """Kodi-Attrappen und geladener Service; die Tests stehen in den Unterklassen."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.visible = {"Player.HasAudio", "Window.IsActive(visualisation)", "Player.ShowInfo"}
        self.settings = {"enabled": "true", "autoopen": "true"}
        self.selected = SPEKTRA
        self.kodi = types.SimpleNamespace(
            Monitor=object, Player=object, LOGINFO=1, LOGDEBUG=0, LOGWARNING=2, LOGERROR=3,
            log=Mock(),
            getGlobalIdleTime=Mock(return_value=3),
            getCondVisibility=lambda name: name in self.visible,
            executeJSONRPC=Mock(side_effect=lambda _r: json.dumps({"result": {"value": self.selected}})),
            executebuiltin=Mock(side_effect=self.builtin),
        )
        addon = Mock()
        addon.getSetting.side_effect = lambda key: self.settings.get(key, "")
        xbmcaddon = types.SimpleNamespace(Addon=Mock(return_value=addon))
        spec = importlib.util.spec_from_file_location("spektra_service", SERVICE)
        self.service = importlib.util.module_from_spec(spec)
        modules = {"xbmc": self.kodi, "xbmcaddon": xbmcaddon, "xbmcvfs": Mock(), "xbmcgui": Mock()}
        with patch.dict(sys.modules, modules), patch.object(sys, "dont_write_bytecode", True):
            spec.loader.exec_module(self.service)
        for name in [n for n in sys.modules if n == "spektra" or n.startswith("spektra.")]:
            del sys.modules[name]  # spektra.kodi wurde mit den Attrappen geladen

    def builtin(self, command):
        if command == "Action(Info)":
            self.visible.symmetric_difference_update({"Player.ShowInfo"})
        elif command == "ActivateWindow(visualisation)":
            self.visible.add("Window.IsActive(visualisation)")
            self.visible.discard("Window.IsActive(home)")
        elif command == "ActivateWindow(home)":
            self.visible.discard("Window.IsActive(visualisation)")
            self.visible.add("Window.IsActive(home)")
        elif command.startswith("Dialog.Close("):
            dialog = command.split("(")[1].split(",")[0]
            self.visible.discard(f"Window.IsActive({dialog})")


class ServiceTest(ServiceCase):
    # --- automatisches Öffnen ---
    def test_opens_visualisation_and_hides_info(self):
        self.visible = {"Player.HasAudio", "Window.IsActive(home)", "Player.ShowInfo"}
        for _ in range(3):
            self.service.show_visualisation()
        self.assertIn("Window.IsActive(visualisation)", self.visible)
        self.assertNotIn("Player.ShowInfo", self.visible)

    def test_info_is_hidden_without_toggling_back(self):
        self.service.show_visualisation()
        self.service.show_visualisation()
        self.kodi.executebuiltin.assert_called_once_with("Action(Info)")

    def test_respects_both_suppress_flags(self):
        for flag in ("!String.IsEmpty(Window(Home).Property(spektra.suppress))",
                     "!String.IsEmpty(Window(Home).Property(partyvideo.suppress))"):
            with self.subTest(flag=flag):
                self.visible = {"Player.HasAudio", "Window.IsActive(home)", flag}
                self.assertFalse(self.service.takeover_allowed())
                self.service.show_visualisation()
                self.kodi.executebuiltin.assert_not_called()

    def test_other_visualisation_is_left_alone(self):
        self.selected = "visualization.partyvideo"
        self.visible = {"Player.HasAudio", "Window.IsActive(home)"}
        self.service.show_visualisation()
        self.kodi.executebuiltin.assert_not_called()

    def test_disabled_or_no_autoopen_or_no_audio_does_nothing(self):
        for settings, visible in (({"enabled": "false", "autoopen": "true"}, {"Player.HasAudio"}),
                                  ({"enabled": "true", "autoopen": "false"}, {"Player.HasAudio"}),
                                  ({"enabled": "true", "autoopen": "true"}, set())):
            with self.subTest(settings=settings):
                self.settings, self.visible = settings, visible | {"Window.IsActive(home)"}
                self.service.show_visualisation()
                self.kodi.executebuiltin.assert_not_called()

    def test_dialogs_and_settings_stay_open(self):
        self.visible = {"Player.HasAudio", "Window.IsActive(home)", "System.HasModalDialog"}
        self.service.show_visualisation()
        self.kodi.executebuiltin.assert_not_called()
        self.visible = {"Player.HasAudio", "Window.IsActive(settings)"}
        self.assertFalse(self.service.idle_return_due())

    def test_idle_return_after_three_seconds(self):
        self.visible = {"Player.HasAudio", "Window.IsActive(home)"}
        self.kodi.getGlobalIdleTime.return_value = 2
        self.assertFalse(self.service.idle_return_due())
        self.kodi.getGlobalIdleTime.return_value = 3
        self.assertTrue(self.service.idle_return_due())

    # --- Befehlskanal ---
    def test_monitor_queues_own_commands_and_reports_settings_changes(self):
        commands, changed = queue.Queue(), threading.Event()
        monitor = self.service.Monitor(commands, changed)
        monitor.onNotification(SPEKTRA, "Other.spektra_cmd", json.dumps({"action": "status"}))
        monitor.onNotification("other.addon", "Other.spektra_cmd", json.dumps({"action": "off"}))
        monitor.onNotification(SPEKTRA, "Other.spektra_cmd", "{kaputt")
        self.assertEqual(commands.get_nowait(), {"action": "status"})
        self.assertTrue(commands.empty())
        monitor.onSettingsChanged()
        self.assertTrue(changed.is_set())

    def test_messages_become_commands_or_errors(self):
        controller = Mock()
        self.service.handle_message({"action": "status"}, controller)
        self.assertEqual(controller.command.call_args.args[0].action, "status")
        self.service.handle_message({"error": "invalid_command"}, controller)
        controller.fail.assert_called_with("invalid_command")
        self.service.handle_message({"error": "fremd"}, controller)
        controller.fail.assert_called_with("invalid_command")
        self.service.handle_message({"action": "explode"}, controller)
        controller.fail.assert_called_with("invalid_command")
        self.assertEqual(controller.command.call_count, 1)

    def test_settings_change_only_syncs(self):
        controller = Mock()
        changed = threading.Event()
        changed.set()
        self.service.handle_settings_change(changed, controller)
        controller.sync.assert_called_once_with()
        self.assertFalse(changed.is_set())
        controller.command.assert_not_called()


class RecoveryTest(ServiceCase):
    def make(self, beat=None):
        path = Path(self.temp.name) / "renderer.json"
        if beat is not None:
            path.write_text(json.dumps({"state": "running", "time": beat}))
        self.now, self.wall = 0.0, 5000.0
        recovery = self.service.VisualisationRecovery(path, lambda: self.now, lambda: self.wall)
        self.assertFalse(recovery.tick())  # Fenster gerade gesehen
        return recovery, path

    def test_missing_heartbeat_reopens_at_most_twice(self):
        recovery, _ = self.make()
        self.now = 4.9
        self.assertFalse(recovery.tick())
        for _attempt in (1, 2):
            self.now += 5.2
            self.assertTrue(recovery.tick())  # -> Hauptmenü
            self.assertTrue(recovery.tick())  # -> wieder Visualisierung
            self.assertIn("Window.IsActive(visualisation)", self.visible)
            self.assertFalse(recovery.tick())  # neues Öffnen beobachtet
        self.now += 10
        self.assertFalse(recovery.tick())
        self.assertEqual(self.kodi.executebuiltin.call_count, 4)

    def test_fresh_heartbeat_never_reopens(self):
        recovery, path = self.make(beat=5000)
        self.now = 6.0
        self.assertFalse(recovery.tick())
        self.kodi.executebuiltin.assert_not_called()

    def test_confirmed_renderer_is_not_polled_again(self):
        recovery, _ = self.make(beat=5000)
        self.now = 6.0
        self.assertFalse(recovery.tick())  # Herzschlag bestätigt
        self.kodi.executeJSONRPC.reset_mock()
        for step in range(50):
            self.now = 7.0 + step
            self.assertFalse(recovery.tick())
        self.kodi.executeJSONRPC.assert_not_called()

    def test_reset_allows_new_attempts(self):
        recovery, _ = self.make()
        recovery.attempts = 2
        recovery.reset()
        self.assertEqual(recovery.attempts, 0)

    def test_no_reopen_while_suppressed(self):
        recovery, _ = self.make()
        self.visible.add("!String.IsEmpty(Window(Home).Property(spektra.suppress))")
        self.now = 6.0
        self.assertFalse(recovery.tick())
        self.kodi.executebuiltin.assert_not_called()


if __name__ == "__main__":
    unittest.main()
