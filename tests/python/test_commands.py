import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "visualization.spektra" / "resources" / "lib"))

from spektra.commands import Command, CommandError, parse_args  # noqa: E402
from spektra.statefile import read_json, write_json  # noqa: E402


class CommandTests(unittest.TestCase):
    def test_parses_bot_arguments(self):
        self.assertEqual(parse_args([]), Command("menu"))
        self.assertEqual(parse_args(["action=on"]), Command("on"))
        self.assertEqual(parse_args(["action=set", "key=intensity", "value=40"]),
                         Command("set", key="intensity", value="40"))
        self.assertEqual(parse_args(["action=set", "key=route.dust", "value="]),
                         Command("set", key="route.dust", value=""))
        self.assertEqual(parse_args(["action=scene", "name=eclipse"]), Command("scene", name="eclipse"))

    def test_round_trip_through_notification(self):
        for command in (Command("status"), Command("set", key="eclipse.drop", value="false"),
                        Command("set", key="route.dust", value=""), Command("scene", name="eclipse")):
            with self.subTest(command=command):
                self.assertEqual(Command.from_data(json.loads(json.dumps(command.to_data()))), command)

    def test_rejects_malformed_commands(self):
        for args in (["action=explode"], ["action=set", "key=intensity"], ["action=set", "value=1"],
                     ["action=on", "key=intensity"], ["action=scene"], ["action=status", "name=x"],
                     ["action"], ["action=on", "action=off"], ["action=on", "farbe=rot"]):
            with self.subTest(args=args):
                with self.assertRaises(CommandError) as ctx:
                    parse_args(args)
                self.assertEqual(ctx.exception.code, "invalid_command")
        for data in (None, [], {"action": 1}, {"action": "on", "extra": "x"}):
            with self.subTest(data=data):
                with self.assertRaises(CommandError):
                    Command.from_data(data)


class StatefileTests(unittest.TestCase):
    def test_write_is_atomic_and_read_is_forgiving(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sub" / "live.json"
            write_json(path, {"revision": 1})
            self.assertEqual(read_json(path), {"revision": 1})
            self.assertFalse(path.with_suffix(".json.tmp").exists())
            path.write_text("[1]")
            self.assertEqual(read_json(path), {})
            path.write_text("{")
            self.assertEqual(read_json(path), {})
            self.assertEqual(read_json(Path(tmp) / "fehlt.json"), {})


if __name__ == "__main__":
    unittest.main()
