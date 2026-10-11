#!/usr/bin/env python3
"""Exercise corpus-driver process failures without requiring a compiled Nix."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


DRIVER = Path(__file__).resolve().parents[2] / "maintainers/check-parser-corpus.py"


class CorpusDriverTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="nix-corpus-driver-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.corpus = self.root / "corpus"
        self.corpus.mkdir()
        for name in ["bad.nix", "good.nix"]:
            (self.corpus / name).write_text("42\n")
        self.output = self.root / "reports/result.json"
        self.checker = self.root / "checker"
        self.checker.write_text(f"#!{sys.executable}\n" + '''
import json, os, sys, time
from pathlib import Path
mode = os.environ["TEST_CHECKER_MODE"]
failed = False
for path in sys.argv[1:]:
    bad = Path(path).name == "bad.nix"
    if bad and mode == "crash":
        sys.exit(2)
    if bad and mode == "timeout":
        time.sleep(10)
    if bad and mode == "malformed":
        print("{broken json")
        sys.exit(0)
    row = {"file": path, "match": not (bad and mode == "difference"), "accepted": True}
    if bad and mode == "wrong-file":
        row["file"] = "another.nix"
    print(json.dumps(row), flush=True)
    failed |= not row["match"]
sys.exit(int(failed))
''')
        self.checker.chmod(0o755)

    def run_driver(self, mode, *extra):
        return subprocess.run(
            [sys.executable, str(DRIVER), str(self.corpus), "--checker", str(self.checker),
             "--output", str(self.output), "--timeout", "1", *extra],
            env={**os.environ, "TEST_CHECKER_MODE": mode}, capture_output=True, text=True,
            timeout=15)

    def test_matching_corpus_and_nested_output_directory(self):
        result = self.run_driver("match")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(self.output.read_text())
        self.assertEqual(report["files"], 2)
        self.assertEqual(report["differences"], [])
        self.assertEqual(report["expected_differences"], [])
        self.assertEqual(len(Path(report["journal"]).read_text().splitlines()), 2)

    def test_process_failures_preserve_other_results(self):
        for mode in ["crash", "timeout", "malformed", "wrong-file", "difference"]:
            with self.subTest(mode=mode):
                result = self.run_driver(mode)
                self.assertEqual(result.returncode, 1, result.stderr)
                report = json.loads(self.output.read_text())
                self.assertEqual(report["files"], 2)
                self.assertEqual(len(report["differences"]), 1)
                failure = report["differences"][0]
                self.assertEqual(Path(failure["file"]).name, "bad.nix")
                if mode != "difference":
                    self.assertIn("fatal", failure)

    def test_rejects_conflicting_output_paths(self):
        for output in [self.corpus / "good.nix", self.checker, self.root / "result.jsonl"]:
            with self.subTest(output=output):
                original = output.read_bytes() if output.exists() else None
                result = self.run_driver("match", "--output", str(output))
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(output.read_bytes() if output.exists() else None, original)

    def test_rejects_missing_checker_and_invalid_limits(self):
        for args in [("--checker", str(self.root / "missing")), ("--timeout", "nan"),
                     ("--timeout", "0"), ("--jobs", "0"), ("--batch-size", "0")]:
            with self.subTest(args=args):
                self.assertEqual(self.run_driver("match", *args).returncode, 2)
                self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
