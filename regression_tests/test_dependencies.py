import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import dependencies


class DependencyTests(unittest.TestCase):
    def test_pinned_checkout_repeatability_and_dirty_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            origin = root / "origin"
            origin.mkdir()

            def git(*args):
                return subprocess.check_output(
                    ["git", "-C", str(origin), *args], text=True, stderr=subprocess.STDOUT
                ).strip()

            git("init")
            git("config", "user.name", "Dependency regression")
            git("config", "user.email", "test@example.invalid")
            source = origin / "source.txt"
            source.write_text("first", encoding="ascii")
            git("add", "source.txt")
            git("commit", "-m", "first")
            first = git("rev-parse", "HEAD")
            source.write_text("second", encoding="ascii")
            git("commit", "-am", "second")
            second = git("rev-parse", "HEAD")
            manifest = {"test": {"url": str(origin), "path": "libs/test", "revision": first}}
            (root / "dependencies.json").write_text(json.dumps(manifest), encoding="utf-8")
            dependencies.fetch(root)
            checkout = root / "libs" / "test"
            self.assertEqual((checkout / "source.txt").read_text(), "first")
            dependencies.fetch(root)
            (checkout / "source.txt").write_text("keep my edit", encoding="ascii")
            manifest["test"]["revision"] = second
            (root / "dependencies.json").write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "dirty"):
                dependencies.fetch(root)
            self.assertEqual((checkout / "source.txt").read_text(), "keep my edit")
            (checkout / "source.txt").write_text("first", encoding="ascii")
            sentinel = checkout / "untracked.txt"
            sentinel.write_text("keep", encoding="ascii")
            with self.assertRaisesRegex(RuntimeError, "dirty"):
                dependencies.fetch(root)
            sentinel.unlink()
            dependencies.fetch(root)
            self.assertEqual((checkout / "source.txt").read_text(), "second")

    def test_non_repository_directory_and_moving_revision_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkout = root / "libs" / "test"
            checkout.mkdir(parents=True)
            manifest = {"test": {"url": "unused", "path": "libs/test", "revision": "main"}}
            (root / "dependencies.json").write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "full commit hash"):
                dependencies.fetch(root)
            manifest["test"]["revision"] = "a" * 40
            (root / "dependencies.json").write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaises(subprocess.CalledProcessError):
                dependencies.fetch(root)


if __name__ == "__main__":
    unittest.main()
