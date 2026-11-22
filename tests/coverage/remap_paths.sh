#!/usr/bin/env bash
set -euo pipefail

# Usage: remap_paths.sh FROM_DIR TO_DIR FILE [FILE...]
# Rewrites every path under FROM_DIR into the same path under TO_DIR, in place, in the coverage reports given (the
# lcov tracefile, the llvm-cov show text). force-cover only appends its markers to existing lines, so a line of the
# mirror IS the same line of the source : the report then names the real sources, which is what Sonar and the html
# read

FROM_DIR="${1:?usage: remap_paths.sh FROM_DIR TO_DIR FILE...}"
TO_DIR="${2:?usage: remap_paths.sh FROM_DIR TO_DIR FILE...}"
shift 2
FROM_DIR="${FROM_DIR%/}"
TO_DIR="${TO_DIR%/}"

for file in "$@"; do
  if [ ! -f "${file}" ]; then
    echo "[remap] missing ${file}" >&2
    exit 1
  fi
  FROM="${FROM_DIR}/" TO="${TO_DIR}/" perl -pi -e 's/\Q$ENV{FROM}\E/$ENV{TO}/g' "${file}"
  echo "[remap] ${file} : ${FROM_DIR} -> ${TO_DIR} ($(grep -c "${TO_DIR}/" "${file}" || true) lines naming the sources)"
done
