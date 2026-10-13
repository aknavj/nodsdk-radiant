"""Restore the editor's pinned native-model build dependencies."""

import json
from pathlib import Path
import re
import subprocess
import sys


def git(path, *args):
    return subprocess.check_output(
        ["git", "-C", str(path), *args], text=True, stderr=subprocess.STDOUT
    ).strip()


def fetch(root=None):
    root = Path(root or Path(__file__).resolve().parent).resolve()
    manifest = json.loads((root / "dependencies.json").read_text(encoding="utf-8"))
    for name, dependency in manifest.items():
        revision = dependency["revision"]
        if not re.fullmatch(r"[0-9a-f]{40}", revision):
            raise ValueError(f"{name}: dependency revision must be a full commit hash")
        path = (root / dependency["path"]).resolve()
        path.relative_to(root)
        if path == root:
            raise ValueError(f"{name}: dependency cannot use the source root")

        existed = path.exists()
        if existed:
            if Path(git(path, "rev-parse", "--show-toplevel")).resolve() != path:
                raise RuntimeError(f"{name}: {path} is not an independent Git checkout")
            if git(path, "status", "--porcelain", "--untracked-files=all"):
                raise RuntimeError(f"{name}: refusing to change dirty checkout {path}")
            if git(path, "remote", "get-url", "origin") != dependency["url"]:
                raise RuntimeError(f"{name}: unexpected origin in {path}")
            if git(path, "rev-parse", "HEAD") == revision:
                print(f"{name}: already at {revision}")
                continue
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            subprocess.check_call(
                ["git", "clone", "--no-checkout", "--depth", "1", dependency["url"], str(path)]
            )

        available = subprocess.run(
            ["git", "-C", str(path), "cat-file", "-e", revision + "^{commit}"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        if available.returncode:
            subprocess.check_call(["git", "-C", str(path), "fetch", "--depth", "1", "origin", revision])
        subprocess.check_call(["git", "-C", str(path), "checkout", "--detach", revision])
        if git(path, "rev-parse", "HEAD") != revision:
            raise RuntimeError(f"{name}: checkout did not resolve to {revision}")
        print(f"{name}: restored {revision}")


if __name__ == "__main__":
    try:
        fetch()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Dependency setup failed: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.output:
            print(error.output, file=sys.stderr)
        sys.exit(1)
