import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "visualization.spektra" / "resources" / "lib"))

from spektra import settings  # noqa: E402
from spektra.settings import SettingError  # noqa: E402


class SettingsTests(unittest.TestCase):
    def test_defaults_match_spec(self):
        d = settings.defaults()
        self.assertEqual(d["intensity"], 70)
        self.assertEqual(d["eclipse.pixel"], 2)
        self.assertEqual(d["maxparticles"], 42000)
        self.assertIs(d["eclipse.drop"], True)
        self.assertEqual(d["scene"], "eclipse")
        expected = {
            "breath": "bass", "flame": "low", "corona": "mid", "free": "high", "flow": "high",
            "wave": "bass", "burst": "mid", "jet": "low", "streamer": "mid", "dust": "high",
        }
        for effect in settings.EFFECTS:
            for band in settings.BANDS:
                self.assertIs(d[f"eclipse.route.{effect}.{band}"], expected[effect] == band, (effect, band))
        self.assertEqual(len(d), 10 + 40 + 1)

    def test_settings_xml_matches_schema(self):
        xml = (ROOT / "visualization.spektra" / "resources" / "settings.xml").read_text(encoding="utf-8")
        found = dict(re.findall(r'<setting id="([^"]+)"[^>]*default="([^"]+)"', xml))
        self.assertEqual(set(found), set(settings.SCHEMA))
        for key, text in found.items():
            self.assertEqual(settings.to_text(key, settings.defaults()[key]), text, key)

    def test_parse_accepts_kodi_and_bot_forms(self):
        self.assertEqual(settings.parse("intensity", "40"), 40)
        self.assertIs(settings.parse("eclipse.drop", "false"), False)
        self.assertIs(settings.parse("eclipse.drop", "1"), True)
        self.assertEqual(settings.parse("scene", "eclipse"), "eclipse")

    def test_rejects_bad_values(self):
        for key, text in (("intensity", "0"), ("intensity", "101"), ("intensity", "viel"),
                          ("intensity", "4.5"), ("eclipse.drop", "ja"), ("scene", "matrix"),
                          ("eclipse.pixel", "9")):
            with self.subTest(key=key, text=text):
                with self.assertRaises(SettingError) as ctx:
                    settings.parse(key, text)
                self.assertEqual(ctx.exception.code, "invalid_value")
        with self.assertRaises(SettingError) as ctx:
            settings.parse("farbe", "rot")
        self.assertEqual(ctx.exception.code, "unknown_key")

    def test_route_shorthand_expands_to_four_switches(self):
        self.assertEqual(
            settings.expand("route.wave", "bass,high"),
            {"eclipse.route.wave.bass": True, "eclipse.route.wave.low": False,
             "eclipse.route.wave.mid": False, "eclipse.route.wave.high": True},
        )
        self.assertEqual(set(settings.expand("route.dust", "").values()), {False})
        # Addons.ExecuteAddon trennt Argumente an Kommas: "+" funktioniert auch über JSON-RPC.
        self.assertEqual(settings.expand("route.wave", "bass+high"),
                         settings.expand("route.wave", "bass,high"))
        with self.assertRaises(SettingError) as ctx:
            settings.expand("route.wave", "bass,treble")
        self.assertEqual(ctx.exception.code, "invalid_value")
        with self.assertRaises(SettingError) as ctx:
            settings.expand("route.blitz", "bass")
        self.assertEqual(ctx.exception.code, "unknown_key")
        self.assertEqual(settings.expand("intensity", "55"), {"intensity": 55})

    def test_read_falls_back_to_defaults(self):
        stored = {"intensity": "40", "eclipse.flame": "kaputt"}
        values = settings.read(lambda key: stored.get(key, ""))
        self.assertEqual(values["intensity"], 40)
        self.assertEqual(values["eclipse.flame"], 100)
        self.assertIs(values["enabled"], True)


if __name__ == "__main__":
    unittest.main()
