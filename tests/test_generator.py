import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("generate_events", ROOT / "tools/generate_events.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class GeneratorTests(unittest.TestCase):
    def row(self, **kwargs):
        return {"name": "Example", "category": "Meta", "offset": "00:00Z", **kwargs}

    def test_duplicate_rows_merge_starts_keep_first_metadata(self):
        events = generator.compile_events([
            self.row(location="First", offset="00:30Z", repeat="06:00"),
            self.row(location="Second", offset="02:00Z", repeat="06:00"),
            self.row(category="Boss", location="Other category")])
        self.assertEqual(len(events), 2)
        self.assertEqual(events[0]["location"], "First")
        self.assertEqual(events[0]["starts"], [1800, 7200, 23400, 28800, 45000, 50400, 66600, 72000])

    def test_wrapping_repeat_expands_full_utc_day(self):
        event, = generator.compile_events([self.row(offset="23:30Z", repeat="06:00")])
        self.assertEqual(event["starts"], [19800, 41400, 63000, 84600])

    def test_daily_and_missing_repeats_each_have_one_start(self):
        for repeat in [None, "", "1.00:00"]:
            event, = generator.compile_events([self.row(repeat=repeat)])
            self.assertEqual(event["starts"], [0])

    def test_duplicate_occurrences_removed(self):
        row = self.row(repeat="02:00")
        event, = generator.compile_events([row, row])
        self.assertEqual(len(event["starts"]), 12)

    def test_bad_or_unrepresentable_schedules_rejected(self):
        for fields in [{"offset": "24:00Z"}, {"offset": "00:60Z"},
                       {"offset": "12:00"}, {"repeat": "00:00"},
                       {"repeat": "07:00"}, {"repeat": "24:00"},
                       {"repeat": "01:99"}, {"alert": -1}]:
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                generator.compile_events([self.row(**fields)])

    def test_tarir_waypoint_survives_generation(self):
        rows = json.loads((ROOT / "assets/events.json").read_text())
        event = next(e for e in generator.compile_events(rows) if e["name"] == "Battle in Tarir")
        self.assertEqual(event["waypoint"], "[&BN0HAAA=]")

    def test_original_catalog(self):
        source = ROOT / "assets/events.json"
        if not source.exists():
            source = ROOT.parent / "ref/events.json"
        rows = json.loads(source.read_text(encoding="utf-8-sig"))
        events = generator.compile_events(rows)
        self.assertEqual(len(rows), 104)
        self.assertEqual(len(events), 88)
        self.assertEqual(len({event["category"] for event in events}), 7)
        self.assertEqual(generator.render(rows), (ROOT / "generated/events.hpp").read_text())


if __name__ == "__main__":
    unittest.main()
