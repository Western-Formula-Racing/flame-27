#!/usr/bin/env python3
"""Pre-build step: compute a unique build ID, save a patch for dirty trees,
and generate build/build_info.h plus build/build_id.txt."""

import getpass
import hashlib
import os
import socket
import subprocess
import shutil
from datetime import datetime, timezone
from pathlib import Path


def git(*args):
    """Run a git command and return stdout as raw bytes."""
    return subprocess.run(["git", *args], capture_output=True, check=True).stdout

# Work from the repo root no matter where the script is called from
script_dir = "."
root = subprocess.run(
    ["git", "rev-parse", "--show-toplevel"],
    cwd=script_dir, capture_output=True, check=True, text=True,
).stdout.strip()
os.chdir(root)

build = Path("build")
build.mkdir(exist_ok=True)

# if old build files exist, move them to archive
archive = Path("archive")
archive.mkdir(exist_ok=True)
if (build / "build_id").exists:
    old_build_id = (build / "build_id.txt").read_text()
    old_build_id = old_build_id.strip('\n')
    dest = archive / old_build_id
    dest.mkdir(parents=True, exist_ok=True)
    if (build / "build.patch").exists():
        shutil.copy(build / "build.patch",dest / "build.patch")
    if (build / "build_info.h").exists():
        shutil.copy(build / "build_info.h",dest / "build_info.h")

rev = git("rev-parse", "--short", "HEAD").decode().strip()

# Mark untracked (non-ignored) files as intent-to-add so they show up in the diff
subprocess.run(["git", "add", "-N", "."], capture_output=True)

# Remove any stale patch from a previous build
patch_path = build / "build.patch"
patch_path.unlink(missing_ok=True)

# Capture raw bytes so binary diffs and line endings are preserved exactly
diff = git("diff", "HEAD", "--binary")

if diff:
    patch_path.write_bytes(diff)
    phash = hashlib.sha256(diff).hexdigest()[:8]
    build_id = f"{rev}-dirty-{phash}"
else:
    build_id = rev

build_time = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

header = (
    "#pragma once\n"
    f'#define BUILD_ID   "{build_id}"\n'
    f'#define BUILD_USER "{getpass.getuser()}"\n'
    f'#define BUILD_HOST "{socket.gethostname()}"\n'
    f'#define BUILD_TIME "{build_time}"\n'
)
(build / "build_info.h").write_text(header, newline="\n")
(build / "build_id.txt").write_text(build_id + "\n")

print(f"Build ID: {build_id}")