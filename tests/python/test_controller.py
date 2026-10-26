import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "visualization.spektra" / "resources" / "lib"))

from spektra import settings  # noqa: E402
from spektra.commands import Command  # noqa: E402
from spektra.controller import Controller  # noqa: E402
from spektra.statefile import read_json  # noqa: E402


class FakeAdapter:
    def __init__(self):
        self.stored = {key: settings.to_text(key, value) for key, value in settings.defaults().items()}
        self.selected = "visualization.partyvideo"
        self.audio = False
        self.window = False
        self.published = []
        self.writes = []

    def get_setting(self, key):
        return self.stored.get(key, "")

    def set_setting(self, key, text):
        self.writes.append((key, text))
        self.stored[key] = text

    def select(self):
        self.selected = "visualization.spektra"

    def has_audio(self):
        return self.audio

    def visible(self):
        return self.window and self.selected == "visualization.spektra"

    def open_visualisation(self):
        self.window = True

    def close_visualisation(self):
        self.window = False

    def publish(self, status):
        self.published.append(status)


class ControllerTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.live = Path(tmp.name) / "live.json"
        self.adapter = FakeAdapter()
        self.now = 1000.0
        self.controller = Controller(self.adapter, self.live, clock=lambda: self.now)
        self.controller.start()

    def last(self):
        return self.adapter.published[-1]

    def test_start_publishes_and_writes_live_json(self):
        self.assertEqual(self.last()["state"], "on")
        self.assertEqual(self.last()["scene"], "eclipse")
        self.assertEqual(read_json(self.live)["settings"]["intensity"], 70)
        self.assertEqual(read_json(self.live)["revision"], 1000000)

    def test_on_selects_spektra_and_opens_with_music(self):
        self.adapter.stored["enabled"] = "false"
        self.adapter.audio = True
        self.controller.command(Command("on"))
        self.assertEqual(self.adapter.selected, "visualization.spektra")
        self.assertTrue(self.adapter.window)
        self.assertEqual(self.adapter.stored["enabled"], "true")
        self.assertEqual(self.last()["state"], "on")
        self.assertTrue(self.last()["visible"])

    def test_off_closes_and_disables(self):
        self.adapter.select()
        self.adapter.window = True
        self.controller.command(Command("off"))
        self.assertFalse(self.adapter.window)
        self.assertEqual(self.adapter.stored["enabled"], "false")
        self.assertEqual(self.last()["state"], "off")

    def test_toggle_flips(self):
        self.controller.command(Command("toggle"))
        self.assertEqual(self.last()["state"], "off")
        self.controller.command(Command("toggle"))
        self.assertEqual(self.last()["state"], "on")

    def test_set_writes_setting_and_live_json(self):
        self.controller.command(Command("set", key="intensity", value="40"))
        self.assertEqual(self.adapter.stored["intensity"], "40")
        self.assertEqual(read_json(self.live)["settings"]["intensity"], 40)
        self.assertEqual(self.last()["settings"]["intensity"], 40)
        self.controller.command(Command("set", key="route.wave", value="mid"))
        self.assertEqual(self.adapter.stored["eclipse.route.wave.bass"], "false")
        self.assertEqual(self.adapter.stored["eclipse.route.wave.mid"], "true")

    def test_invalid_set_changes_nothing(self):
        cases = (("farbe", "rot", "unknown_key"), ("intensity", "viel", "invalid_value"),
                 ("intensity", "500", "invalid_value"), ("route.wave", "treble", "invalid_value"))
        for key, value, code in cases:
            with self.subTest(key=key, value=value):
                self.adapter.writes.clear()
                self.controller.command(Command("set", key=key, value=value))
                self.assertEqual(self.adapter.writes, [])
                self.assertEqual(self.last()["state"], "error")
                self.assertEqual(self.last()["error"], code)
        self.controller.command(Command("status"))
        self.assertEqual(self.last()["state"], "on")
        self.assertEqual(self.last()["error"], "")

    def test_reset_restores_every_default(self):
        self.controller.command(Command("set", key="intensity", value="10"))
        self.controller.command(Command("set", key="route.burst", value="bass"))
        self.controller.command(Command("reset"))
        self.assertEqual(settings.read(self.adapter.get_setting), settings.defaults())

    def test_scene_accepts_only_known_scenes(self):
        self.controller.command(Command("scene", name="eclipse"))
        self.assertEqual(self.last()["state"], "on")
        self.controller.command(Command("scene", name="matrix"))
        self.assertEqual(self.last()["error"], "invalid_value")

    def test_menu_is_not_a_service_command(self):
        self.controller.command(Command("menu"))
        self.assertEqual(self.last()["error"], "invalid_command")

    def test_open_settings_dialog_does_not_hide_the_new_value(self):
        # Bei offenem Einstellungsdialog speichert Kodi setSetting erst beim Schließen.
        original = self.adapter.set_setting
        self.adapter.set_setting = lambda key, text: self.adapter.writes.append((key, text))
        self.controller.command(Command("set", key="intensity", value="25"))
        self.assertEqual(read_json(self.live)["settings"]["intensity"], 25)
        self.assertEqual(self.last()["settings"]["intensity"], 25)
        self.controller.command(Command("off"))
        self.assertEqual(self.last()["state"], "off")
        self.adapter.set_setting = original

    def test_sync_does_not_write_settings(self):
        self.adapter.writes.clear()
        self.controller.sync()
        self.assertEqual(self.adapter.writes, [])

    def test_revision_is_monotonic_across_restarts(self):
        first = read_json(self.live)["revision"]
        self.controller.sync()
        self.assertGreater(read_json(self.live)["revision"], first)
        self.now = 999.0  # Uhr zurückgestellt
        restarted = Controller(self.adapter, self.live, clock=lambda: self.now)
        restarted.start()
        self.assertGreater(read_json(self.live)["revision"], first + 1)


if __name__ == "__main__":
    unittest.main()
