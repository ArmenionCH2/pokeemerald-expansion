#!/usr/bin/env python3
"""Materialize AtlasGBA's changed game files from an authenticated source checkout.

This is a TRANSFER helper, not a recommendation for final target-repository layout.
The ZIP intentionally contains only exact source references until hydrated. An
agent with GitHub access should run this first and inspect the resulting files.

Usage: python hydrate_game.py --checkout /path/to/atlasgba
Optional: python hydrate_game.py --clone-url https://github.com/despairarc000/atlasgba.git
"""
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent
DEST = ROOT / "game"
BASE = "ae2c4a415eecdfe9cfed53f8a64206bc7c3fc6a9"
HEAD = "4c3e95f3c78aa364e0aa81abe0aa75d7d5893628"
ENGINE = "implementation/engine/pokeemerald/"
ASSETS = "implementation/assets/"


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], stderr=subprocess.PIPE)


def hydrate(repo):
    repo = pathlib.Path(repo).resolve()
    for rev in (BASE, HEAD):
        git(repo, "cat-file", "-e", rev + "^{commit}")
    raw = git(repo, "diff", "--name-status", "-z", "--no-renames", BASE, HEAD,
              "--", ENGINE, ASSETS)
    fields = raw.split(b"\0")
    if fields[-1] != b"":
        raise RuntimeError("Unexpected Git diff record format")
    fields.pop()
    if len(fields) % 2:
        raise RuntimeError("Unexpected Git name-status record count")
    entries = []
    for i in range(0, len(fields), 2):
        status, raw_name = fields[i].decode(), fields[i + 1].decode("utf-8", "surrogateescape")
        if status not in ("A", "M", "D"):
            raise RuntimeError(f"Unsupported change status {status} at {raw_name}")
        if raw_name.startswith(ENGINE):
            rel = raw_name[len(ENGINE):]
        elif raw_name.startswith(ASSETS):
            rel = "assets/" + raw_name[len(ASSETS):]
        else:
            raise RuntimeError(f"Unexpected input path {raw_name}")
        out = (DEST / rel).resolve()
        if not out.is_relative_to(DEST.resolve()):
            raise RuntimeError(f"Unsafe output path {rel}")
        if status == "D":
            entries.append({"path": rel, "status": "deleted_from_original", "source": raw_name})
            continue
        content = git(repo, "show", f"{HEAD}:{raw_name}")
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(content)
        entries.append({"path": rel, "status": "added" if status == "A" else "modified",
                        "source": raw_name, "git_blob": git(repo, "rev-parse", f"{HEAD}:{raw_name}").decode().strip(),
                        "sha256": hashlib.sha256(content).hexdigest(), "bytes": len(content)})
    if not entries:
        raise RuntimeError("No changed game-source entries were found; verify repository and refs")
    record = {"base_commit": BASE, "source_commit": HEAD,
              "source_repository": "despairarc000/atlasgba", "entries": entries}
    (DEST / "EXTRACTED_SOURCE_PROVENANCE.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"Exported {sum(x['status'] != 'deleted_from_original' for x in entries)} added/modified files under {DEST}")
    print("Keep this code for review; do not overwrite Expansion files wholesale.")
    full_zip = ROOT / "AtlasGBA_FULL_GAME_SOURCE.zip"
    with zipfile.ZipFile(full_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(ROOT / "FULL_PLAN.md", "FULL_PLAN.md")
        for p in sorted(DEST.rglob("*")):
            if p.is_file() and p.name != "SOURCE_REF.json":
                z.write(p, p.relative_to(ROOT))
    print(f"Created complete source ZIP: {full_zip}")


def main():
    p = argparse.ArgumentParser()
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument("--checkout", type=pathlib.Path)
    source.add_argument("--clone-url")
    a = p.parse_args()
    if a.checkout:
        hydrate(a.checkout)
    else:
        with tempfile.TemporaryDirectory(prefix="atlas-source-") as d:
            subprocess.run(["git", "clone", "--quiet", a.clone_url, d], check=True)
            hydrate(d)


if __name__ == "__main__":
    main()
