#!/usr/bin/env python3
# Usage: list_test_executables.py CTEST BUILD_DIR CONFIG OUT_FILE
#
# Writes to OUT_FILE the executables the ctest suite of BUILD_DIR runs, one per
# line, each once. The list comes from ctest itself, so it is exactly what ran,
# whatever the generator, the output folders or the names of the targets.
# CONFIG may be empty (single-config generator without a build type).

import json
import os
import subprocess
import sys

if len(sys.argv) != 5:
    sys.exit("usage: list_test_executables.py CTEST BUILD_DIR CONFIG OUT_FILE")

ctest, build_dir, config, out_file = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]

cmd = [ctest, "--test-dir", build_dir, "--show-only=json-v1"]
if config:
    cmd += ["-C", config]
listing = json.loads(subprocess.check_output(cmd))

executables = []
for test in listing.get("tests", []):
    command = test.get("command") or []
    if not command:
        continue
    exe = os.path.realpath(command[0])
    if os.path.isfile(exe) and os.access(exe, os.X_OK) and exe not in executables:
        executables.append(exe)

if not executables:
    sys.exit("[list_test_executables] no test executable found in " + build_dir + " (config '" + config + "')")

with open(out_file, "w") as handle:
    handle.write("\n".join(executables) + "\n")
print("[list_test_executables] " + str(len(executables)) + " executables -> " + out_file)
