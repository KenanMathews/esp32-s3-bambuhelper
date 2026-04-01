#!/usr/bin/env python3
"""
Generate store/index.json and sdk/demos-index.json from Lua header comments.

Reads every .lua file in store/apps/ and sdk/demos/*/app.lua, extracts
-- @tag values, and writes the respective index files.  Run this whenever
a Lua file's metadata changes instead of editing index.json by hand.

Usage:
    python tools/build_index.py

Global defaults (used when a tag is absent from the .lua file):
    @version     1.0
    @author      BambuHelper
    @category    uncategorized
    @description (empty)
    @color       0x18E3
    @sdk_min     1
"""

import json
import re
import glob
import os
import sys
from datetime import date

# ---------------------------------------------------------------------------
#  Configuration
# ---------------------------------------------------------------------------
REPO_BASE  = "https://raw.githubusercontent.com/KenanMathews/esp32-s3-bambuhelper/main"

TAGS = ["name", "version", "color", "sdk_min", "author", "category", "description"]

DEFAULTS = {
    "version":     "1.0",
    "author":      "BambuHelper",
    "category":    "uncategorized",
    "description": "",
    "color":       "0x18E3",
    "sdk_min":     "1",
}

# ---------------------------------------------------------------------------
#  Helpers
# ---------------------------------------------------------------------------
def parse_headers(path):
    """Read first 512 bytes of a .lua file and extract -- @tag values."""
    with open(path, encoding="utf-8") as f:
        header = f.read(512)
    meta = {}
    for tag in TAGS:
        m = re.search(rf"-- @{tag}\s+(.+)", header)
        meta[tag] = m.group(1).strip() if m else DEFAULTS.get(tag, "")
    return meta


def color_to_int(s):
    """Convert '0x07E0' or plain decimal string to int. Returns 0 on failure."""
    try:
        return int(s, 0)
    except (ValueError, TypeError):
        return 0


def build_catalog(lua_paths, id_from_dir, repo_subpath, output_path):
    """
    lua_paths   : list of .lua file paths to process
    id_from_dir : if True, use parent dir name as id; else use filename stem
    repo_subpath: path segment after REPO_BASE for script_url construction
    output_path : where to write the resulting JSON
    """
    apps = []
    errors = 0

    for lua_path in sorted(lua_paths):
        if id_from_dir:
            app_id = os.path.basename(os.path.dirname(lua_path))
            script_url = f"{REPO_BASE}/{repo_subpath}/{app_id}/app.lua"
        else:
            app_id = os.path.basename(lua_path)[:-4]
            script_url = f"{REPO_BASE}/{repo_subpath}/{app_id}.lua"

        meta = parse_headers(lua_path)

        if not meta.get("name"):
            print(f"  SKIP  {app_id}: missing -- @name", file=sys.stderr)
            errors += 1
            continue

        apps.append({
            "id":          app_id,
            "name":        meta["name"],
            "description": meta["description"],
            "author":      meta["author"],
            "category":    meta["category"],
            "version":     meta["version"],
            "sdk_min":     int(meta["sdk_min"]) if meta["sdk_min"].isdigit() else 1,
            "color":       color_to_int(meta["color"]),
            "script_url":  script_url,
        })
        print(f"  OK    {app_id}  v{meta['version']}  ({meta['name']})")

    index = {
        "version": 1,
        "updated": str(date.today()),
        "apps":    apps,
    }

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(index, f, indent=2)
        f.write("\n")

    rel = os.path.relpath(output_path, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    print(f"\nWrote {rel}  ({len(apps)} app{'s' if len(apps) != 1 else ''})\n")
    return errors


# ---------------------------------------------------------------------------
#  Main
# ---------------------------------------------------------------------------
script_dir = os.path.dirname(os.path.abspath(__file__))
repo_root  = os.path.dirname(script_dir)

total_errors = 0

print("── Store apps ──────────────────────────")
total_errors += build_catalog(
    sorted(glob.glob(os.path.join(repo_root, "store/apps/*.lua"))),
    id_from_dir=False,
    repo_subpath="store/apps",
    output_path=os.path.join(repo_root, "store/index.json"),
)

print("── Demo apps ───────────────────────────")
total_errors += build_catalog(
    sorted(glob.glob(os.path.join(repo_root, "sdk/demos/*/app.lua"))),
    id_from_dir=True,
    repo_subpath="sdk/demos",
    output_path=os.path.join(repo_root, "sdk/demos-index.json"),
)

if total_errors:
    print(f"{total_errors} file(s) skipped — see errors above", file=sys.stderr)
    sys.exit(1)
