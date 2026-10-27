import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "visualization.spektra" / "resources" / "lib"))

from spektra import calibration, settings  # noqa: E402

STRINGS = ROOT / "visualization.spektra" / "resources" / "language" / "resource.language.de_de" / "strings.po"


class CalibrationTests(unittest.TestCase):
    def test_items_are_numeric_settings(self):
        for key, _label, step in calibration.ITEMS:
            with self.subTest(key=key):
                self.assertEqual(settings.SCHEMA[key].kind, "int")
                self.assertGreater(step, 0)

    def test_items_cover_every_slider(self):
        keys = {key for key, _label, _step in calibration.ITEMS}
        tunable = {key for key, spec in settings.SCHEMA.items()
                   if spec.kind == "int" and key != "maxparticles"}
        self.assertEqual(keys, tunable)

    def test_labels_exist(self):
        ids = set(re.findall(r'msgctxt "#(\d+)"', STRINGS.read_text(encoding="utf-8")))
        used = [label for _key, label, _step in calibration.ITEMS]
        used += [label for labels in calibration.CHOICES.values() for label in labels]
        for label in used:
            self.assertIn(str(label), ids)

    def test_choices_match_the_value_range(self):
        for key, labels in calibration.CHOICES.items():
            spec = settings.SCHEMA[key]
            self.assertEqual(len(labels), spec.maximum - spec.minimum + 1, key)

    def test_steps_stay_in_range(self):
        self.assertEqual(calibration.stepped("eclipse.size", 100, 1), 105)
        self.assertEqual(calibration.stepped("eclipse.size", 150, 1), 150)
        self.assertEqual(calibration.stepped("eclipse.size", 50, -1), 50)
        self.assertEqual(calibration.stepped("eclipse.pulse", 0, -1), 0)
        self.assertEqual(calibration.stepped("eclipse.palette", 3, 1), 3)

    def test_display_uses_labels_for_choices(self):
        def text(number):
            return f"<{number}>"

        self.assertEqual(calibration.display("eclipse.palette", 1, text), "<32101>")
        self.assertEqual(calibration.display("eclipse.pixel", 2, text), "<32022>")
        self.assertEqual(calibration.display("eclipse.size", 120, text), "120")


if __name__ == "__main__":
    unittest.main()
